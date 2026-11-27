#pragma once
#include "pipeline_catalog.h"

namespace gpu::catalog {
// Rebuilt from the currently selected NSP/extracted RomFS. Owns metadata only.
class GameSources {
public:
    // scan_contents: also find every raw NVN program inside the files (recording needs it, ~0.2 s for the shader
    // packages). Using a catalog only needs the file table: each program is read at its recorded offset and checked.
    bool index(bool scan_contents = true);
    bool locate(const u8* bytes, size_t available, u32 stage, Locator& out) const;
    bool read(const Locator&, std::vector<u8>&) const;
    bool verify() const;  // read-only source-locator roundtrip, writes no code
    size_t count() const { return shaders_.size(); }
private:
    struct Resource { std::string name; u64 offset, size; };
    std::vector<Resource> files_;
    std::vector<Locator> shaders_;
    bool nsp_ = false;
    std::string root_;
    bool resource(const Resource&, std::vector<u8>&) const;
};
} // namespace gpu::catalog
