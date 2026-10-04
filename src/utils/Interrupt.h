#pragma once

namespace sltcd::interrupt {

/// Ask every running conversion to stop at the next record boundary.
///
/// Safe to call from a signal handler and from any thread; the flag is only
/// ever set, never cleared, by the handler itself.
void request() noexcept;

/// True once request() was called: Ctrl+C / Ctrl+Break in a console, SIGTERM,
/// or a programmatic request (tests, embedders).
bool requested() noexcept;

/// Forget a previous request. A run that already stopped stays stopped, so this
/// only exists for tests and for a frontend that starts a fresh run.
void reset() noexcept;

/// Install the console handlers (SIGINT, SIGTERM, SIGBREAK) that set the flag.
///
/// Without them the runtime terminates the process in the middle of a write;
/// with them the run finishes the record it is working on and stops with the
/// exit code sltcd::ErrorCode::Interrupted. Calling this more than once is
/// harmless.
void installHandler();

} // namespace sltcd::interrupt
