#include "utils/UUID.h"

#include <cctype>
#include <stdexcept>

namespace sltcd {
namespace {

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

std::optional<UUID> UUID::fromString(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }

    if (text.size() == 38 && text.front() == '{' && text.back() == '}') {
        text.remove_prefix(1);
        text.remove_suffix(1);
    }
    if (text.size() != 36) {
        return std::nullopt;
    }

    UUID::Bytes bytes{};
    std::size_t byteIndex = 0;
    int highNibble = -1;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '-') {
            if (i != 8 && i != 13 && i != 18 && i != 23) {
                return std::nullopt;
            }
            continue;
        }

        const int value = hexValue(c);
        if (value < 0) {
            return std::nullopt;
        }

        if (highNibble < 0) {
            highNibble = value;
        } else {
            if (byteIndex >= bytes.size()) {
                return std::nullopt;
            }
            bytes[byteIndex] = static_cast<std::uint8_t>((highNibble << 4) | value);
            ++byteIndex;
            highNibble = -1;
        }
    }

    if (byteIndex != bytes.size() || highNibble >= 0) {
        return std::nullopt;
    }
    return UUID(bytes);
}

UUID UUID::parse(std::string_view text) {
    const auto parsed = fromString(text);
    if (!parsed.has_value()) {
        throw std::invalid_argument("invalid UUID string: '" + std::string(text) + "'");
    }
    return *parsed;
}

std::string UUID::toString() const {
    static constexpr char kHexDigits[] = "0123456789abcdef";

    std::string result;
    result.reserve(36);
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            result.push_back('-');
        }
        result.push_back(kHexDigits[bytes_[i] >> 4]);
        result.push_back(kHexDigits[bytes_[i] & 0x0F]);
    }
    return result;
}

} // namespace sltcd
