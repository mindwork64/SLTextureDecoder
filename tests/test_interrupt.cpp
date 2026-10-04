#include <gtest/gtest.h>

#include <csignal>

#include "utils/Interrupt.h"

namespace {

/// A request must never leak into another test, whatever the test does.
class CleanInterrupts {
public:
    CleanInterrupts() { sltcd::interrupt::reset(); }
    ~CleanInterrupts() { sltcd::interrupt::reset(); }
};

} // namespace

TEST(Interrupt, IsNotRequestedUntilAsked) {
    const CleanInterrupts clean;
    EXPECT_FALSE(sltcd::interrupt::requested());
}

TEST(Interrupt, RequestAndResetToggleTheFlag) {
    const CleanInterrupts clean;

    sltcd::interrupt::request();
    EXPECT_TRUE(sltcd::interrupt::requested());

    sltcd::interrupt::reset();
    EXPECT_FALSE(sltcd::interrupt::requested());
}

TEST(Interrupt, InstallHandlerReplacesTheDefaultTermination) {
    const CleanInterrupts clean;

    // Installing twice is harmless: this is what a frontend does when it parses
    // the command line and then starts a run.
    sltcd::interrupt::installHandler();
    sltcd::interrupt::installHandler();

    // The runtime would end the process on Ctrl+C; after the install the flag
    // owning handler is in place, so SIGINT must no longer be the default one.
    void (*previous)(int) = std::signal(SIGINT, SIG_IGN);
    EXPECT_NE(previous, SIG_DFL);
    EXPECT_NE(previous, SIG_ERR);

    std::signal(SIGINT, previous); // put the flag handler back in place
    EXPECT_FALSE(sltcd::interrupt::requested());
}
