#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace sltcd {

/// Error categories. The numeric values double as process exit codes so that a
/// caller can distinguish failures without parsing text (plan section 25/26).
enum class ErrorCode : int {
    Ok = 0,
    Usage = 1,
    CacheNotFound = 2,
    EntryNotFound = 3,
    CacheTooSmall = 4,
    SizeMismatch = 5,
    DecodeError = 6,
    WriteError = 7,
    IoError = 8,
    InvalidFormat = 9,
};

/// Base class for all recoverable errors raised by the core.
class Error : public std::runtime_error {
public:
    Error(ErrorCode code, std::string message) : std::runtime_error(std::move(message)), code_(code) {}

    ErrorCode code() const noexcept { return code_; }

    /// Ready-to-print, human readable description.
    virtual std::string toUserMessage() const { return what(); }

private:
    ErrorCode code_;
};

class CacheNotFound : public Error {
public:
    explicit CacheNotFound(std::string message) : Error(ErrorCode::CacheNotFound, std::move(message)) {}
};

class EntryNotFound : public Error {
public:
    explicit EntryNotFound(std::string message) : Error(ErrorCode::EntryNotFound, std::move(message)) {}
};

class CacheTooSmall : public Error {
public:
    explicit CacheTooSmall(std::string message) : Error(ErrorCode::CacheTooSmall, std::move(message)) {}
};

class SizeMismatch : public Error {
public:
    explicit SizeMismatch(std::string message) : Error(ErrorCode::SizeMismatch, std::move(message)) {}
};

class DecodeError : public Error {
public:
    explicit DecodeError(std::string message) : Error(ErrorCode::DecodeError, std::move(message)) {}
};

class WriteError : public Error {
public:
    explicit WriteError(std::string message) : Error(ErrorCode::WriteError, std::move(message)) {}
};

class IoError : public Error {
public:
    explicit IoError(std::string message) : Error(ErrorCode::IoError, std::move(message)) {}
};

class InvalidFormat : public Error {
public:
    explicit InvalidFormat(std::string message) : Error(ErrorCode::InvalidFormat, std::move(message)) {}
};

} // namespace sltcd
