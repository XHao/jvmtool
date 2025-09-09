/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modified by: shako
 * - Added metaspace structures test
 * - This test validates the extended metaspace functionality
 */

#include "metaspace_structs.h"
#include <iostream>
#include <iomanip>

namespace jvmtool {

void testMetaspaceStructs() {
    std::cout << "=== Metaspace Structures Test ===" << std::endl;
    
    // Test basic functionality
    std::cout << "Has metaspace structs: " << (MetaspaceStructs::hasMetaspaceStructs() ? "Yes" : "No") << std::endl;
    std::cout << "Has shared metaspace: " << (MetaspaceStructs::hasSharedMetaspace() ? "Yes" : "No") << std::endl;
    
    if (MetaspaceStructs::hasSharedMetaspace()) {
        void* base = MetaspaceStructs::sharedMetaspaceBase();
        void* top = MetaspaceStructs::sharedMetaspaceTop();
        size_t size = MetaspaceStructs::sharedMetaspaceSize();
        
        std::cout << "Shared metaspace base: 0x" << std::hex << base << std::endl;
        std::cout << "Shared metaspace top:  0x" << std::hex << top << std::endl;
        std::cout << "Shared metaspace size: " << std::dec << size << " bytes (" 
                  << (size / 1024 / 1024) << " MB)" << std::endl;
        
        // Test object checking with some sample addresses
        void* test_addr1 = base;
        void* test_addr2 = static_cast<char*>(base) + size / 2;
        void* test_addr3 = top;
        void* test_addr4 = static_cast<char*>(top) + 1;
        
        std::cout << "Test address in shared metaspace:" << std::endl;
        std::cout << "  0x" << std::hex << test_addr1 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr1) ? "Yes" : "No") << std::endl;
        std::cout << "  0x" << std::hex << test_addr2 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr2) ? "Yes" : "No") << std::endl;
        std::cout << "  0x" << std::hex << test_addr3 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr3) ? "Yes" : "No") << std::endl;
        std::cout << "  0x" << std::hex << test_addr4 << ": " 
                  << (MetaspaceStructs::isSharedMetaspaceObject(test_addr4) ? "Yes" : "No") << std::endl;
    } else {
        std::cout << "Shared metaspace not available (CDS may not be enabled)" << std::endl;
    }
    
    std::cout << "=== Test Complete ===" << std::endl;
}

} // namespace jvmtool
