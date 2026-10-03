#pragma once

#include <string>

namespace sltcd {

/// Version of the OpenJPEG library actually linked at runtime.
std::string openjpegVersion();

/// Version of the libpng library actually linked at runtime.
std::string libpngVersion();

/// Version of this tool ("major.minor.patch").
std::string toolVersion();

} // namespace sltcd
