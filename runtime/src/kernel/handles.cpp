// The kernel handle table. Moved text-for-text from kernel.cpp so the unit tests can link it alone (they stub
// g_kernel_lock and current_thread()).
#include "kernel.h"

namespace kern {

// HOS-like handle values (non-zero, low bits set): (index << 15) | 0x10 with a 17-bit index, so the value fits in
// 32 bits. The index wraps after 131071 handles (FEW opens one per short-lived thread, about every frame); values
// still open are skipped, so a wrap never replaces a live handle.
Handle HandleTable::add(ObjectPtr obj) {
    KGuard g(g_kernel_lock);
    constexpr u32 kIndexEnd = 1u << 17;
    for (u32 tries = 1; tries < kIndexEnd; tries++) {
        Handle h = (next_ << 15) | 0x10;
        next_ = next_ + 1 == kIndexEnd ? 1 : next_ + 1;
        if (table_.count(h)) continue;
        table_[h] = std::move(obj);
        return h;
    }
    hw_fatal("handle table full");
    return 0;
}
ObjectPtr HandleTable::get(Handle h) const {
    KGuard g(g_kernel_lock);
    if (h == kCurrentThread) {
        Thread* t = current_thread();
        // The current thread's object is kept alive by its own handle entry.
        for (auto& [k, v] : table_)
            if (v.get() == t) return v;
        return nullptr;
    }
    auto it = table_.find(h);
    return it == table_.end() ? nullptr : it->second;
}
bool HandleTable::close(Handle h) {
    KGuard g(g_kernel_lock);
    return table_.erase(h) != 0;
}

}  // namespace kern
