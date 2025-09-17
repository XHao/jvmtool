#pragma once

#include "vm/codeCache.h"

namespace jvmtool {

class LibraryLoader {
  public:
    static CodeCache* findLibraryByName(const char* libName);
};

}  // namespace jvmtool
