#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sltcd {

/// 128-bit identifier used by Second Life for assets.
///
/// The 16 bytes are stored in canonical (big-endian) order, i.e. exactly the
/// order that appears in the textual form "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
/// This was verified against a real texture.entries file, where the bytes of a
/// record matched its "<uuid>.texture" file name one to one.
class UUID {
public:
    using Bytes = std::array<std::uint8_t, 16>;

    UUID() = default;
    explicit UUID(const Bytes& bytes) noexcept : bytes_(bytes) {}

    /// Parse the canonical textual form. Returns std::nullopt for malformed input.
    static std::optional<UUID> fromString(std::string_view text);

    /// Parse the canonical textual form, throwing std::invalid_argument on failure.
    static UUID parse(std::string_view text);

    /// Build an identifier from exactly 16 raw bytes (canonical order).
    static UUID fromBytes(const Bytes& bytes) noexcept { return UUID(bytes); }

    const Bytes& bytes() const noexcept { return bytes_; }

    /// Lowercase canonical textual form, e.g. "4a2e79d1-1b0e-0f94-f06e-d1244d3d0a4c".
    std::string toString() const;

    friend bool operator==(const UUID& lhs, const UUID& rhs) noexcept { return lhs.bytes_ == rhs.bytes_; }
    friend bool operator!=(const UUID& lhs, const UUID& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator<(const UUID& lhs, const UUID& rhs) noexcept { return lhs.bytes_ < rhs.bytes_; }

private:
    Bytes bytes_{};
};

} // namespace sltcd

namespace std {

template<>
struct hash<sltcd::UUID> {
    std::size_t operator()(const sltcd::UUID& id) const noexcept {
        // FNV-1a over the raw bytes; stable and good enough for hash maps.
        std::size_t result = 1469598103934665603ULL;
        for (const std::uint8_t byte : id.bytes()) {
            result ^= static_cast<std::size_t>(byte);
            result *= 1099511628211ULL;
        }
        return result;
    }
};

} // namespace std
