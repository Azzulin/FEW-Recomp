// FEW Recomp Setup (Windows): pick your own game files, build the game, play. One window, one main button whose label
// follows the state, as HWDER's setup did (docs/windows-setup-ux.md) and other recomps' installers do:
//
//   Your game files  ->  [Build the game]  ->  progress (Cancel)  ->  [Play] [Desktop shortcut] [Open game folder]
//
// It only drives gerar-few.ps1 (the same script as a build by hand) and shows its progress from the events file
// (tools/build_events.py). Nothing of the game is in this program, and no file is copied: the game reads them where
// they are. Practices kept from HWDER's setup, which antivirus programs had flagged: no administrator rights, an icon,
// a manifest and version information, and PowerShell started visibly (minimized), never hidden.
#include "setup.h"
#include "ui.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

namespace {
namespace fs = std::filesystem;
using u64 = unsigned long long;

// ---- The game's files ---------------------------------------------------------------------------------------------

constexpr u64 kBaseUs = 0x0100F15003E64000ull, kBaseJp = 0x0100EAE003A5A000ull;  // tools/game_profile.py
enum class Kind { Missing, Unreadable, Base, Update, Dlc, Other };

struct NspInfo {
    Kind kind = Kind::Missing;
    u64 base = 0;  // the base game it belongs to
};

u64 hex16(const std::string& s, size_t at) {
    if (s.size() < at + 16) return 0;
    u64 v = 0;
    for (size_t i = at; i < at + 16; i++) {
        char c = s[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return 0;
        v = v << 4 | u64(d);
    }
    return v;
}

// The title ID: from the NSP's ticket (<rights id>.tik, whose first 16 hex digits are the title ID; the package's
// file table is not encrypted), else from a [0100...] in the file name.
u64 title_id(const std::string& path) {
    u64 id = 0;
    if (SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb")) {
        unsigned char head[16];
        if (SDL_ReadIO(io, head, 16) == 16 && !memcmp(head, "PFS0", 4)) {
            Uint32 count, strings;
            memcpy(&count, head + 4, 4), memcpy(&strings, head + 8, 4);
            if (count < 1024 && strings < 65536) {
                std::string table(size_t(count) * 24 + strings, '\0');
                if (SDL_ReadIO(io, table.data(), table.size()) == table.size()) {
                    const char* names = table.data() + size_t(count) * 24;
                    for (Uint32 i = 0; i < count && !id; i++) {
                        Uint32 at;
                        memcpy(&at, table.data() + size_t(i) * 24 + 16, 4);
                        if (at >= strings) continue;
                        std::string name(names + at, strnlen(names + at, strings - at));
                        if (name.size() == 36 && name.compare(32, 4, ".tik") == 0) id = hex16(name, 0);
                    }
                }
            }
        }
        SDL_CloseIO(io);
    }
    if (!id) {
        std::string name = fs::u8path(path).filename().u8string();
        for (size_t i = name.find('['); !id && i != std::string::npos; i = name.find('[', i + 1))
            if (i + 17 < name.size() && name[i + 17] == ']') id = hex16(name, i + 1);
    }
    return id;
}

NspInfo classify(const std::string& path) {
    NspInfo info;
    std::error_code ec;
    if (path.empty() || !fs::is_regular_file(fs::u8path(path), ec)) return info;
    u64 id = title_id(path);
    info.kind = Kind::Other;
    if (!id) info.kind = Kind::Unreadable;
    for (u64 base : {kBaseUs, kBaseJp}) {
        if (id == base) info.kind = Kind::Base, info.base = base;
        else if (id == (base | 0x800)) info.kind = Kind::Update, info.base = base;
        else if ((id & ~0xFFFull) == base + 0x1000) info.kind = Kind::Dlc, info.base = base;
    }
    return info;
}

const char* region(u64 base) { return base == kBaseJp ? "Japanese release" : "US release"; }

bool looks_like_keys(const std::string& path) {  // reads the file but never shows any of it
    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
    if (!io) return false;
    std::string text(4096, '\0');
    text.resize(SDL_ReadIO(io, text.data(), text.size()));
    SDL_CloseIO(io);
    for (auto& c : text) c = char(tolower((unsigned char)c));
    return text.find("header_key") != std::string::npos;
}

std::vector<std::string> nsps_in(const std::string& dir, int depth = 0) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::u8path(dir), ec)) {
        std::error_code e2;
        if (e.is_directory(e2) && depth < 1) {
            auto sub = nsps_in(e.path().u8string(), depth + 1);
            out.insert(out.end(), sub.begin(), sub.end());
        } else if (e.is_regular_file(e2)) {
            std::string ext = e.path().extension().u8string();
            for (auto& c : ext) c = char(tolower((unsigned char)c));
            if (ext == ".nsp") out.push_back(e.path().u8string());
        }
    }
    return out;
}

std::string known_folder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = narrow(p);
    CoTaskMemFree(p);
    return out;
}

// ---- What the player picked (remembered across versions in %LOCALAPPDATA%\FEW-Recomp\setup.ini) -------------------

struct Inputs {
    std::string base, update, keys, dlc_dir;
    bool operator==(const Inputs& o) const { return base == o.base && update == o.update && keys == o.keys && dlc_dir == o.dlc_dir; }
};

std::map<std::string, std::string> read_ini(const std::string& path) {
    std::map<std::string, std::string> out;
    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
    if (!io) return out;
    std::string text(size_t(std::max<Sint64>(SDL_GetIOSize(io), 0)), '\0');
    text.resize(SDL_ReadIO(io, text.data(), text.size()));
    SDL_CloseIO(io);
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) j = text.size();
        std::string line = text.substr(i, j - i);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        size_t eq = line.find('=');
        if (eq != std::string::npos && line[0] != '#') out[line.substr(0, eq)] = line.substr(eq + 1);
        i = j + 1;
    }
    return out;
}

void write_ini(const std::string& path, const std::map<std::string, std::string>& values) {
    std::error_code ec;
    fs::create_directories(fs::u8path(path).parent_path(), ec);
    std::string text = "# FEW Recomp Setup: the files you picked (paths only; nothing is copied)\n";
    for (auto& [k, v] : values) text += k + "=" + v + "\n";
    if (SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "wb")) {
        SDL_WriteIO(io, text.data(), text.size());
        SDL_CloseIO(io);
    }
}

// ---- The build: gerar-few.ps1 in a job object (Cancel ends it and everything it started) ---------------------------

struct Job {
    HANDLE job = nullptr, process = nullptr;
    std::thread reader;
    std::mutex m;
    std::deque<std::string> tail;  // the last lines it printed
    std::atomic<bool> running{false};
    DWORD exit_code = 0;

    void reset() {
        if (reader.joinable()) reader.join();
        if (process) CloseHandle(process), process = nullptr;
        if (job) CloseHandle(job), job = nullptr;
        tail.clear();
    }
    void cancel() {
        if (job) TerminateJobObject(job, 1);
    }
    std::string last_line() {
        std::lock_guard<std::mutex> l(m);
        for (auto it = tail.rbegin(); it != tail.rend(); ++it)
            if (it->find_first_not_of(" \t") != std::string::npos) return *it;
        return {};
    }
};

std::wstring quote_arg(const std::string& s) { return L"\"" + wide(s) + L"\""; }

// ---- The setup itself ---------------------------------------------------------------------------------------------

enum class Stage { Files, Building, Done, Failed };
enum class Pick { None, Base, Update, Keys, DlcDir, Folder };

struct Setup {
    std::string kit, release = "dev", settings_path, events_path, log_path, stamp_path, game_dir, game_exe;
    Inputs in;
    Stage stage = Stage::Files;
    Build build = Build::planned();
    EventsFile events;
    Job job;
    std::string message;  // a note under the buttons
    bool focus_main = true, confirm_close = false;
    double next_poll = 0;
    // File dialogs answer on another thread.
    std::mutex dialog_m;
    Pick dialog_target = Pick::None;
    std::vector<std::string> dialog_result;
    bool dialog_open = false;
    // Checks, redone when a path changes.
    Inputs checked;
    NspInfo base_info, update_info;
    bool keys_ok = false;
    int dlc_count = 0;
    std::string dlc_where;
};
Setup* g = nullptr;

std::string kit_file(const char* name) { return (fs::u8path(g->kit) / fs::u8path(name)).u8string(); }

void save_inputs() {
    auto values = read_ini(g->settings_path);
    values["base"] = g->in.base, values["update"] = g->in.update, values["keys"] = g->in.keys, values["dlc_dir"] = g->in.dlc_dir;
    write_ini(g->settings_path, values);
}

std::string stamp() {  // what a finished build was made from
    return g->release + "|" + g->in.base + "|" + g->in.update + "|" + g->in.keys + "|" + g->in.dlc_dir;
}

void recheck() {
    if (g->checked == g->in && g->checked.base != "\x01") return;
    g->checked = g->in;
    g->base_info = classify(g->in.base);
    g->update_info = classify(g->in.update);
    g->keys_ok = !g->in.keys.empty() && looks_like_keys(g->in.keys);
    g->dlc_count = 0;
    std::string dir = g->in.dlc_dir;
    if (dir.empty() && !g->in.base.empty()) dir = fs::u8path(g->in.base).parent_path().u8string();
    g->dlc_where = dir;
    if (!dir.empty() && g->base_info.kind == Kind::Base)
        for (const auto& p : nsps_in(dir, 1)) {
            NspInfo i = classify(p);
            if (i.kind == Kind::Dlc && i.base == g->base_info.base) g->dlc_count++;
        }
}

bool inputs_ready() {
    recheck();
    return g->base_info.kind == Kind::Base && g->update_info.kind == Kind::Update &&
           g->update_info.base == g->base_info.base && g->keys_ok;
}

// A folder with everything in it: the base, its update and DLCs, and a prod.keys there or in the usual places.
void use_folder(const std::string& dir) {
    std::string base_us, base_jp, update_us, update_jp, dlc_dir;
    for (const auto& p : nsps_in(dir)) {
        NspInfo i = classify(p);
        if (i.kind == Kind::Base) (i.base == kBaseJp ? base_jp : base_us) = p;
        else if (i.kind == Kind::Update) (i.base == kBaseJp ? update_jp : update_us) = p;
        else if (i.kind == Kind::Dlc && dlc_dir.empty()) dlc_dir = fs::u8path(p).parent_path().u8string();
    }
    bool jp = base_us.empty() && !base_jp.empty();
    if (!(jp ? base_jp : base_us).empty()) g->in.base = jp ? base_jp : base_us;
    if (!(jp ? update_jp : update_us).empty()) g->in.update = jp ? update_jp : update_us;
    std::string base_dir = g->in.base.empty() ? std::string() : fs::u8path(g->in.base).parent_path().u8string();
    g->in.dlc_dir = dlc_dir == base_dir ? std::string() : dlc_dir;
    std::vector<std::string> keys = {(fs::u8path(dir) / "prod.keys").u8string(),
                                     (fs::u8path(dir).parent_path() / "prod.keys").u8string(),
                                     (fs::u8path(known_folder(FOLDERID_Profile)) / ".switch" / "prod.keys").u8string(),
                                     kit_file("prod.keys")};
    for (const auto& k : keys)
        if (looks_like_keys(k)) { g->in.keys = k; break; }
    save_inputs();
}

void SDLCALL dialog_done(void*, const char* const* files, int) {
    std::lock_guard<std::mutex> l(g->dialog_m);
    g->dialog_result.clear();
    if (files)
        for (; *files; ++files) g->dialog_result.push_back(*files);
    g->dialog_open = false;
}

void open_dialog(SDL_Window* window, Pick target) {
    {
        std::lock_guard<std::mutex> l(g->dialog_m);
        if (g->dialog_open) return;
        g->dialog_open = true, g->dialog_target = target, g->dialog_result.clear();
    }
    static const SDL_DialogFileFilter nsp[] = {{"Nintendo Switch package (*.nsp)", "nsp"}, {"All files", "*"}};
    static const SDL_DialogFileFilter keys[] = {{"Keys (prod.keys)", "keys;txt"}, {"All files", "*"}};
    std::string start = !g->in.base.empty() ? fs::u8path(g->in.base).parent_path().u8string() : known_folder(FOLDERID_Downloads);
    const char* where = start.empty() ? nullptr : start.c_str();
    if (target == Pick::Folder || target == Pick::DlcDir) SDL_ShowOpenFolderDialog(dialog_done, nullptr, window, where, false);
    else if (target == Pick::Keys) SDL_ShowOpenFileDialog(dialog_done, nullptr, window, keys, 2, where, false);
    else SDL_ShowOpenFileDialog(dialog_done, nullptr, window, nsp, 2, where, false);
}

void take_dialog_result() {
    std::lock_guard<std::mutex> l(g->dialog_m);
    if (g->dialog_open || g->dialog_target == Pick::None) return;
    Pick target = g->dialog_target;
    g->dialog_target = Pick::None;
    if (g->dialog_result.empty()) return;  // cancelled
    const std::string& p = g->dialog_result[0];
    switch (target) {
    case Pick::Base: g->in.base = p; break;
    case Pick::Update: g->in.update = p; break;
    case Pick::Keys: g->in.keys = p; break;
    case Pick::DlcDir: g->in.dlc_dir = p; break;
    case Pick::Folder: use_folder(p); return;
    default: break;
    }
    save_inputs();
}

void start_build() {
    g->job.reset();
    g->message.clear();
    std::error_code ec;
    fs::create_directories(fs::u8path(g->events_path).parent_path(), ec);
    if (SDL_IOStream* io = SDL_IOFromFile(g->events_path.c_str(), "wb")) SDL_CloseIO(io);  // empty: a new build
    g->events.reset();
    g->build = Build::planned();
    fs::remove(fs::u8path(g->stamp_path), ec);
    SetEnvironmentVariableW(L"FEW_EVENTS", wide(g->events_path).c_str());

    std::wstring ps = wide(known_folder(FOLDERID_System)) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring cmd = L"\"" + ps + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " + quote_arg(kit_file("gerar-few.ps1")) +
                       L" -NoUi -BaseNsp " + quote_arg(g->in.base) + L" -UpdateNsp " + quote_arg(g->in.update) + L" -Keys " +
                       quote_arg(g->in.keys);
    if (!g->in.dlc_dir.empty()) cmd += L" -DlcDir " + quote_arg(g->in.dlc_dir);

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE out_r = nullptr, out_w = nullptr;
    if (!CreatePipe(&out_r, &out_w, &sa, 0)) { g->message = "Could not start the build (pipe)."; return; }
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWMINNOACTIVE;  // visible, minimized: never a hidden PowerShell
    si.hStdOutput = si.hStdError = out_w;
    si.hStdInput = nullptr;
    PROCESS_INFORMATION pi{};
    std::wstring line = cmd;
    BOOL ok = CreateProcessW(ps.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
                             nullptr, wide(g->kit).c_str(), &si, &pi);
    CloseHandle(out_w);
    if (!ok) {
        CloseHandle(out_r);
        g->message = "Could not start PowerShell (error " + std::to_string(GetLastError()) + ").";
        return;
    }
    g->job.job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (g->job.job) {
        SetInformationJobObject(g->job.job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
        AssignProcessToJobObject(g->job.job, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    g->job.process = pi.hProcess;
    g->job.running = true;
    std::string log = g->log_path;
    Job* job = &g->job;
    job->reader = std::thread([job, out_r, log] {
        FILE* f = _wfopen(wide(log).c_str(), L"wb");
        char buf[4096];
        DWORD n = 0;
        std::string partial;
        while (ReadFile(out_r, buf, sizeof buf, &n, nullptr) && n) {
            if (f) fwrite(buf, 1, n, f), fflush(f);
            partial.append(buf, n);
            size_t nl;
            std::lock_guard<std::mutex> l(job->m);
            while ((nl = partial.find('\n')) != std::string::npos) {
                std::string text = partial.substr(0, nl);
                if (!text.empty() && text.back() == '\r') text.pop_back();
                job->tail.push_back(text);
                if (job->tail.size() > 200) job->tail.pop_front();
                partial.erase(0, nl + 1);
            }
        }
        if (f) fclose(f);
        CloseHandle(out_r);
    });
    g->stage = Stage::Building;
    g->focus_main = true;
}

void poll_build() {
    g->events.poll(g->build);
    if (!g->job.running || WaitForSingleObject(g->job.process, 0) != WAIT_OBJECT_0) return;
    GetExitCodeProcess(g->job.process, &g->job.exit_code);
    g->job.running = false;
    if (g->job.reader.joinable()) g->job.reader.join();
    g->events.poll(g->build);  // its last words
    if (g->job.exit_code == 0 && g->build.outcome == Outcome::Done) {
        write_ini(g->stamp_path, {{"built", stamp()}});
        g->stage = Stage::Done;
    } else {
        if (g->build.outcome != Outcome::Failed) {
            g->build.outcome = Outcome::Failed;
            if (g->build.error.empty()) g->build.error = g->job.last_line();
        }
        g->stage = Stage::Failed;
    }
    g->focus_main = true;
}

// ---- Drawing --------------------------------------------------------------------------------------------------------

// One input row: a status diamond and line, the path under it, and a Browse button on the right.
void file_row(Frame& f, SDL_Window* window, const char* label, const std::string& path, bool ok, const std::string& status,
              Pick pick, const char* button = "Browse...") {
    ImGui::PushID(label);
    float y = ImGui::GetCursorScreenPos().y, bw = 150 * f.scale;
    const bool optional = pick == Pick::DlcDir;  // DLCs: nothing found is fine, never an error
    ImU32 color = ok ? kGreen : (path.empty() || optional) ? kFaint : kCrimson;
    diamond(f.dl, ImVec2(f.cx + 7 * f.scale, y + f.fs * 0.55f), 6 * f.scale, color, ok);
    ImGui::SetCursorScreenPos(ImVec2(f.cx + 24 * f.scale, y));
    ImGui::TextColored(vec(kIvory), "%s", label);
    ImGui::SameLine();
    ImGui::TextColored(vec(ok ? kGreen : (path.empty() || optional) ? kMuted : kCrimson), "  %s", status.c_str());
    ImGui::SetCursorScreenPos(ImVec2(f.cx + 24 * f.scale, ImGui::GetCursorScreenPos().y));
    ImGui::PushFont(nullptr, f.fb * 0.8f);
    std::string shown = path.empty() ? (pick == Pick::DlcDir ? "" : "not chosen yet") : path;
    float room = f.cw - bw - 40 * f.scale;
    while (shown.size() > 8 && ImGui::CalcTextSize(shown.c_str()).x > room) shown = "..." + shown.substr(std::min<size_t>(shown.size(), 8));
    ImGui::TextColored(vec(kMuted), "%s", shown.c_str());
    ImGui::PopFont();
    float after = ImGui::GetCursorScreenPos().y;
    ImGui::SetCursorScreenPos(ImVec2(f.cx + f.cw - bw, y));
    if (gold_button(button, ImVec2(bw, f.fs * 1.75f), false)) open_dialog(window, pick);
    ImGui::SetCursorScreenPos(ImVec2(f.cx, std::max(after - 6 * f.scale, y + f.fs * 2.0f) + 6 * f.scale));
    ImGui::PopID();
}

std::string nsp_status(const NspInfo& i, Kind want, u64 base) {
    switch (i.kind) {
    case Kind::Missing: return "";
    case Kind::Unreadable: return "not a Nintendo Switch package this setup can read";
    case Kind::Base: return want == Kind::Base ? std::string("Fire Emblem Warriors, ") + region(i.base) : "this is the base game, not the update";
    case Kind::Update:
        if (want != Kind::Update) return "this is the update, not the base game";
        if (base && i.base != base) return "an update for the other release of the game";
        return region(i.base);  // the version (1.5.0) is checked when the build unpacks it
    case Kind::Dlc: return "this is a DLC";
    default: return "not Fire Emblem Warriors";
    }
}

void draw_files(Frame& f, SDL_Window* window) {
    recheck();
    ImGui::PushFont(nullptr, f.fb * 1.45f);
    ImGui::TextColored(vec(kGoldLight), "Your game files");
    ImGui::PopFont();
    ImGui::TextColored(vec(kMuted), "Your own dump. The files stay where they are: nothing is copied or sent anywhere.");
    if (gold_button("Choose a folder with all of them...", ImVec2(std::min(f.cw, 420 * f.scale), f.fs * 1.9f), false))
        open_dialog(window, Pick::Folder);
    ImGui::Dummy(ImVec2(0, 4 * f.scale));
    const u64 base = g->base_info.kind == Kind::Base ? g->base_info.base : 0;
    file_row(f, window, "Base game (.nsp)", g->in.base, g->base_info.kind == Kind::Base,
             nsp_status(g->base_info, Kind::Base, 0), Pick::Base);
    bool update_ok = g->update_info.kind == Kind::Update && (!base || g->update_info.base == base);
    file_row(f, window, "Update 1.5.0 (.nsp)", g->in.update, update_ok, nsp_status(g->update_info, Kind::Update, base),
             Pick::Update);
    file_row(f, window, "Keys (prod.keys)", g->in.keys, g->keys_ok,
             g->in.keys.empty() ? "" : g->keys_ok ? "found" : "not a prod.keys file", Pick::Keys);
    std::string dlc = g->dlc_count ? std::to_string(g->dlc_count) + (g->dlc_count == 1 ? " DLC found" : " DLCs found")
                                   : "none found (optional)";
    file_row(f, window, "DLCs (optional)", g->dlc_where, g->dlc_count > 0, dlc, Pick::DlcDir, "Choose folder...");

    // The main button sits at the bottom of the panel, always visible, as in an installer.
    const float bh = 52 * f.scale, bottom = f.main_b.y - 40 * f.scale;
    ImGui::SetCursorScreenPos(ImVec2(f.cx, std::max(ImGui::GetCursorScreenPos().y, bottom - bh - f.fs * 1.6f)));
    bool ready = inputs_ready();
    std::string missing;
    for (auto [have, name] : {std::pair<bool, const char*>{g->base_info.kind == Kind::Base, "the base game"},
                              {update_ok, "update 1.5.0"}, {g->keys_ok, "your prod.keys"}})
        if (!have) missing += (missing.empty() ? "" : ", ") + std::string(name);
    ImGui::PushFont(nullptr, f.fb * 0.85f);
    if (ready) ImGui::TextColored(vec(kMuted), "The first build downloads its tools once (about 1 GB) and takes about 20-45 minutes.");
    else ImGui::TextColored(vec(kGoldLight), "Still needed: %s.", missing.c_str());
    ImGui::PopFont();
    if (g->focus_main && ready) { ImGui::SetKeyboardFocusHere(); g->focus_main = false; }
    if (gold_button("Build the game", ImVec2(std::min(f.cw * 0.5f, 300 * f.scale), bh), true, ready)) start_build();
    bool built = fs::exists(fs::u8path(g->game_exe));
    if (built) {
        ImGui::SameLine(0, 16 * f.scale);
        if (gold_button("Back to Play", ImVec2(200 * f.scale, bh), false)) g->stage = Stage::Done;
    }
    if (!g->message.empty()) ImGui::TextColored(vec(kCrimson), "%s", g->message.c_str());
}

bool make_shortcut() {
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&link))) return false;
    link->SetPath(wide(g->game_exe).c_str());
    link->SetWorkingDirectory(wide(g->game_dir).c_str());
    link->SetDescription(L"Fire Emblem Warriors (FEW Recomp)");
    IPersistFile* file = nullptr;
    bool ok = SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&file));
    if (ok) {
        std::wstring where = wide(known_folder(FOLDERID_Desktop)) + L"\\Fire Emblem Warriors.lnk";
        ok = SUCCEEDED(file->Save(where.c_str(), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}

void draw_done(Frame& f) {
    ImGui::PushFont(nullptr, f.fb * 1.6f);
    ImGui::TextColored(vec(kGoldLight), "Your game is ready");
    ImGui::PopFont();
    if (g->build.started)
        ImGui::TextColored(vec(kMuted), "Built in %s. Start it from here, from a desktop shortcut, or with few.exe in the "
                                        "game folder.", duration(std::max(0.0, g->build.last_t - g->build.start_t)).c_str());
    else
        ImGui::TextColored(vec(kMuted), "Start it from here, from a desktop shortcut, or with few.exe in the game folder.");
    float by = ImGui::GetCursorScreenPos().y + 22 * f.scale;
    progress_bar(f.dl, ImVec2(f.cx, by), ImVec2(f.cx + f.cw, by + 34 * f.scale), 1.0, f.t, f.scale);
    ImGui::SetCursorScreenPos(ImVec2(f.cx, by + 66 * f.scale));
    float gap = 16 * f.scale, bh = 56 * f.scale, room = f.cw - 2 * gap;
    float w_play = std::min(room * 0.30f, 240 * f.scale), w_short = std::min(room * 0.38f, 300 * f.scale);
    float w_open = std::min(room - w_play - w_short, 300 * f.scale);
    if (g->focus_main) { ImGui::SetKeyboardFocusHere(); g->focus_main = false; }
    if (gold_button("Play", ImVec2(w_play, bh), true)) {
        if (!start_game(g->game_exe, g->game_dir)) g->message = "The game could not be started: " + g->game_exe;
    }
    ImGui::SameLine(0, gap);
    if (gold_button("Desktop shortcut", ImVec2(w_short, bh), false))
        g->message = make_shortcut() ? "Shortcut \"Fire Emblem Warriors\" added to the desktop." : "The shortcut could not be made.";
    ImGui::SameLine(0, gap);
    if (gold_button("Open game folder", ImVec2(w_open, bh), false)) open_path(g->game_dir);
    if (!g->message.empty()) ImGui::TextColored(vec(g->message.rfind("Shortcut", 0) == 0 ? kGreen : kCrimson), "%s", g->message.c_str());
    ImGui::Dummy(ImVec2(0, 14 * f.scale));
    ImGui::TextColored(vec(kMuted), "Game files: %s", fs::u8path(g->in.base).filename().u8string().c_str());
    if (gold_button("Change files or build again", ImVec2(std::min(f.cw, 340 * f.scale), f.fs * 2.0f), false)) {
        g->stage = Stage::Files, g->message.clear(), g->focus_main = true;
    }
}

void draw_failed(Frame& f) {
    ImGui::PushFont(nullptr, f.fb * 1.6f);
    ImGui::TextColored(vec(kCrimson), "The build stopped");
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::TextColored(vec(kIvory), "%s", g->build.error.empty() ? "See the build log for the reason." : g->build.error.c_str());
    ImGui::Spacing();
    ImGui::TextColored(vec(kMuted), "Building again continues where it stopped. If it stops at the same place, attach the "
                                    "build log when you report it.");
    ImGui::Dummy(ImVec2(0, 14 * f.scale));
    if (g->focus_main) { ImGui::SetKeyboardFocusHere(); g->focus_main = false; }
    float bh = 52 * f.scale, gap = 16 * f.scale;
    if (gold_button("Build again", ImVec2(220 * f.scale, bh), true)) start_build();
    ImGui::SameLine(0, gap);
    if (gold_button("Open the build log", ImVec2(240 * f.scale, bh), false)) open_path(g->log_path);
    ImGui::SameLine(0, gap);
    if (gold_button("Change files", ImVec2(200 * f.scale, bh), false)) g->stage = Stage::Files, g->focus_main = true;
}

}  // namespace

bool setup_init(SDL_Window*, std::string& error) {
    static Setup setup;
    g = &setup;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // The kit: the folder with gerar-few.ps1, next to this program or up to three folders above (a build of it).
    const char* base = SDL_GetBasePath();
    fs::path dir = fs::u8path(base ? base : ".");
    if (!dir.has_filename()) dir = dir.parent_path();  // SDL ends it with a separator
    for (int i = 0; i < 4 && !dir.empty(); i++, dir = dir.parent_path())
        if (fs::exists(dir / "gerar-few.ps1")) { g->kit = dir.u8string(); break; }
    if (g->kit.empty()) {
        error = "gerar-few.ps1 was not found next to this program. Extract the whole FEW Recomp zip into a new folder "
                "and run FEW Recomp Setup.exe from there.";
        return false;
    }
    if (SDL_IOStream* io = SDL_IOFromFile(kit_file("RELEASE").c_str(), "rb")) {
        char buf[64] = {};
        SDL_ReadIO(io, buf, sizeof buf - 1);
        SDL_CloseIO(io);
        g->release = buf;
        while (!g->release.empty() && isspace((unsigned char)g->release.back())) g->release.pop_back();
        if (g->release.empty()) g->release = "dev";
    }
    g->settings_path = (fs::u8path(known_folder(FOLDERID_LocalAppData)) / "FEW-Recomp" / "setup.ini").u8string();
    g->events_path = kit_file("build\\build-events.jsonl");
    g->events.path = g->events_path;
    g->log_path = kit_file("build\\setup-build.log");
    g->stamp_path = kit_file("build\\setup-built.ini");
    g->game_dir = kit_file("Jogar no PC");
    g->game_exe = kit_file("Jogar no PC\\few.exe");
    auto values = read_ini(g->settings_path);
    g->in = {values["base"], values["update"], values["keys"], values["dlc_dir"]};
    if (g->in.base.empty()) {  // first run: the kit's "Game Files" folder, if the player made one as the guide says
        std::error_code ec;
        if (fs::is_directory(fs::u8path(kit_file("Game Files")), ec)) use_folder(kit_file("Game Files"));
    }
    if (const char* folder = getenv("FEW_SETUP_FOLDER"); folder && *folder) use_folder(folder);  // tests: as if picked
    g->checked.base = "\x01";
    std::error_code ec;
    if (fs::exists(fs::u8path(g->game_exe), ec) && read_ini(g->stamp_path)["built"] == stamp()) g->stage = Stage::Done;
    if (const char* a = getenv("FEW_SETUP_AUTOBUILD"); a && *a && strcmp(a, "0") && inputs_ready()) start_build();  // tests
    return true;
}

bool setup_frame(SDL_Window* window, float scale, bool close_requested) {
    take_dialog_result();
    double now = SDL_GetTicks() / 1000.0;
    if (g->stage == Stage::Building && now >= g->next_poll) {
        poll_build();
        g->next_poll = now + 0.25;
        if (g->stage != Stage::Building) SDL_FlashWindow(window, SDL_FLASH_UNTIL_FOCUSED);
    }
    if (close_requested) {
        if (g->stage != Stage::Building) return false;
        g->confirm_close = true;
    }

    const bool failed = g->stage == Stage::Failed;
    std::string top = g->release;
    if (g->stage == Stage::Building && g->build.started)
        top = "Elapsed  " + duration(std::max(0.0, now_seconds() - g->build.start_t));
    Frame f = begin_frame(scale, "Build the PC version from your own copy of the game", top, failed);
    draw_steps(f, g->build);
    begin_main(f);
    switch (g->stage) {
    case Stage::Files: draw_files(f, window); break;
    case Stage::Building:
        draw_progress(f, g->build, "Starting...");
        if (gold_button("Cancel", ImVec2(160 * f.scale, 48 * f.scale), false)) {
            g->job.cancel();
            g->build.error = "Cancelled. Building again continues where it stopped.";
        }
        break;
    case Stage::Done: draw_done(f); break;
    case Stage::Failed: draw_failed(f); break;
    }
    end_main(f);

    bool keep = true;
    if (g->confirm_close) { ImGui::OpenPopup("Stop the build?"); g->confirm_close = false; }
    if (ImGui::BeginPopupModal("Stop the build?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(vec(kIvory), "Closing stops the build. Building again later continues where it stopped.");
        ImGui::Dummy(ImVec2(0, 8 * scale));
        if (gold_button("Keep building", ImVec2(220 * scale, 48 * scale), true)) ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, 16 * scale);
        if (gold_button("Stop and close", ImVec2(220 * scale, 48 * scale), false)) keep = false;
        ImGui::EndPopup();
    }
    const char* footer = g->stage == Stage::Building
        ? "Closing this window stops the build; building again continues where it stopped. Details: build\\setup-build.log"
        : "No administrator rights needed. Your files are read where they are; nothing of the game is in this program.";
    end_frame(f, footer);
    return keep;
}

bool setup_busy() { return g && g->stage == Stage::Building; }

void setup_shutdown() {
    if (!g) return;
    if (g->job.running) g->job.cancel();
    g->job.reset();
    CoUninitialize();
}
