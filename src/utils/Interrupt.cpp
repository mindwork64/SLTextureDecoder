#include "utils/Interrupt.h"

#include <atomic>
#include <csignal>

namespace sltcd::interrupt {
namespace {

std::atomic<bool> g_requested{false};

/// A signal handler may neither allocate, lock nor log: setting one lock free
/// flag is all it does, and the run loop polls it between records.
extern "C" void handleSignal(int /*signal*/) {
    g_requested.store(true, std::memory_order_relaxed);
}

} // namespace

void request() noexcept {
    g_requested.store(true, std::memory_order_relaxed);
}

bool requested() noexcept {
    return g_requested.load(std::memory_order_relaxed);
}

void reset() noexcept {
    g_requested.store(false, std::memory_order_relaxed);
}

void installHandler() {
    static const bool installed = []() {
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);
#ifdef SIGBREAK
        std::signal(SIGBREAK, handleSignal); // Ctrl+Break on Windows consoles
#endif
        return true;
    }();
    (void)installed;
}

} // namespace sltcd::interrupt
