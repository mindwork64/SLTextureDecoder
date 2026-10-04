#include <gtest/gtest.h>

#include <string>

#include "utils/Errors.h"

TEST(Errors, ExposeCategoryCodes) {
    EXPECT_EQ(static_cast<int>(sltcd::ErrorCode::Ok), 0);
    EXPECT_EQ(sltcd::CacheNotFound("x").code(), sltcd::ErrorCode::CacheNotFound);
    EXPECT_EQ(sltcd::EntryNotFound("x").code(), sltcd::ErrorCode::EntryNotFound);
    EXPECT_EQ(sltcd::CacheTooSmall("x").code(), sltcd::ErrorCode::CacheTooSmall);
    EXPECT_EQ(sltcd::SizeMismatch("x").code(), sltcd::ErrorCode::SizeMismatch);
    EXPECT_EQ(sltcd::DecodeError("x").code(), sltcd::ErrorCode::DecodeError);
    EXPECT_EQ(sltcd::WriteError("x").code(), sltcd::ErrorCode::WriteError);
    EXPECT_EQ(sltcd::IoError("x").code(), sltcd::ErrorCode::IoError);
    EXPECT_EQ(sltcd::InvalidFormat("x").code(), sltcd::ErrorCode::InvalidFormat);
    // An interrupted run is not an error: the codes are handed out to the
    // process exit code unchanged, so the value is part of the interface.
    EXPECT_EQ(static_cast<int>(sltcd::ErrorCode::Interrupted), 10);
}

TEST(Errors, CarryMessageAndAreCatchableAsBaseClass) {
    try {
        throw sltcd::CacheTooSmall("texture.cache is truncated");
    } catch (const sltcd::Error& error) {
        EXPECT_EQ(error.code(), sltcd::ErrorCode::CacheTooSmall);
        EXPECT_EQ(std::string(error.what()), "texture.cache is truncated");
        EXPECT_EQ(error.toUserMessage(), "texture.cache is truncated");
    } catch (...) {
        FAIL() << "sltcd::Error did not catch its own derived class";
    }
}

TEST(Errors, DeriveFromStdExceptionForUnifiedCatching) {
    try {
        throw sltcd::DecodeError("codestream rejected");
    } catch (const std::exception& error) {
        EXPECT_EQ(std::string(error.what()), "codestream rejected");
    }
}
