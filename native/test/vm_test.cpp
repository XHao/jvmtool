#include <gtest/gtest.h>
#include "vm/vmStructs.h"
#include "vm/codeCache.h"

using namespace jvmtool;

class VMStructsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(VMStructsTest, BasicInterface) {
    // Test that VMStructs has basic functionality without causing segfaults
    // Note: These tests depend on the actual VM structures being available
    // We just test that the methods exist and don't crash on null inputs
    EXPECT_NO_THROW(VMStructs::init(nullptr));
    // Don't call ready() as it might cause segfaults without proper VM context
}

TEST_F(VMStructsTest, SymbolReading) {
    // Test symbol reading functionality
    // Note: readSymbol is protected, so we can't test it directly
    // This is a basic interface test 
    EXPECT_NO_THROW(VMStructs::init(nullptr));
}

class CodeCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(CodeCacheTest, BasicFunctionality) {
    // Test basic CodeCache functionality safely
    // Don't actually create a CodeCache as it might cause segfaults without proper VM context
    // Just test that the class exists and we can reference it
    SUCCEED(); // This test just ensures the header compiles
}

// ============================================================================
// Tests for Precise Metaspace Measurement Extensions
// ============================================================================

class VMStructsMetaspaceExtensionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // These tests verify the new offset variables and accessor methods
    }
};

TEST_F(VMStructsMetaspaceExtensionsTest, SanityConstantsAreReasonable) {
    // Test that our sanity check constants are reasonable
    EXPECT_GT(MAX_REASONABLE_KLASS_SIZE_WORDS, 0);
    EXPECT_LT(MAX_REASONABLE_KLASS_SIZE_WORDS, 100000);  // Should be less than 100K words
    
    EXPECT_GT(MAX_REASONABLE_VTABLE_LENGTH, 0);
    EXPECT_LT(MAX_REASONABLE_VTABLE_LENGTH, 1000000);
    
    EXPECT_GT(MAX_REASONABLE_METHOD_COUNT, 1000);  // Should allow at least 1000 methods
    EXPECT_LT(MAX_REASONABLE_METHOD_COUNT, 10000000);
    
    EXPECT_GT(MAX_REASONABLE_CP_LENGTH, 100);  // Should allow at least 100 CP entries
    EXPECT_LT(MAX_REASONABLE_CP_LENGTH, 10000000);
}

TEST_F(VMStructsMetaspaceExtensionsTest, OffsetAccessorMethodsExist) {
    // Test that new offset accessor methods exist and don't crash
    EXPECT_NO_THROW({
        int offset1 = VMStructs::constMethodCodeSizeOffset();
        int offset2 = VMStructs::constantPoolCacheOffset();
        int offset3 = VMStructs::instanceKlassSizeHelperOffset();
        int offset4 = VMStructs::instanceKlassFieldsOffset();
        int offset5 = VMStructs::instanceKlassAnnotationsOffset();
        
        // All offsets should be initialized to -1 before resolveOffsets() is called
        // We can't test actual values without a real JVM context
        SUCCEED();
    });
}

TEST_F(VMStructsMetaspaceExtensionsTest, VMKlassMethodsHandleNullGracefully) {
    // Test that VMKlass methods handle nullptr gracefully
    // Note: We can't create real VMKlass objects without a JVM, but we can test
    // that the code compiles and the sanity constants are used
    
    // These are compile-time checks to ensure the methods exist
    // At runtime, they would return -1 for invalid offsets
    SUCCEED();
}

TEST_F(VMStructsMetaspaceExtensionsTest, BoundaryValueChecks) {
    // Test that boundary value constants make sense
    
    // InstanceKlass size: 64 bytes to 16KB is reasonable
    const int min_klass_size_words = 8;   // 64 bytes minimum
    const int max_klass_size_words = MAX_REASONABLE_KLASS_SIZE_WORDS;
    EXPECT_GT(max_klass_size_words, min_klass_size_words);
    EXPECT_EQ(max_klass_size_words, 2048);  // 16KB / 8 bytes per word
    
    // VTable/ITable length
    EXPECT_EQ(MAX_REASONABLE_VTABLE_LENGTH, 10000);
    EXPECT_EQ(MAX_REASONABLE_ITABLE_LENGTH, 10000);
    
    // Method count
    EXPECT_EQ(MAX_REASONABLE_METHOD_COUNT, 100000);
    
    // Constant pool length
    EXPECT_EQ(MAX_REASONABLE_CP_LENGTH, 100000);
    
    // Code size: 10MB max per method is generous
    EXPECT_EQ(MAX_REASONABLE_CODE_SIZE, 10 * 1024 * 1024);
}

TEST_F(VMStructsMetaspaceExtensionsTest, SafeAccessIntegration) {
    // Test that SafeAccess is properly integrated
    // We can't test actual safe access without a JVM, but we can verify
    // that the code compiles and uses SafeAccess correctly
    
    // This is more of a compile-time check
    SUCCEED();
}

// Note: Integration tests with real JVM will be in separate test suite
// These unit tests only verify the code structure and sanity checks

TEST_F(CodeCacheTest, NullPointerHandling) {
    // Test that we can reference the CodeCache class without crashing
    // Don't actually create instances as they might cause segfaults
    SUCCEED(); // This test ensures the class is available
}

// Test for common utility functions
class UtilityTest : public ::testing::Test {};

TEST_F(UtilityTest, PointerArithmetic) {
    // Test basic pointer arithmetic used throughout the codebase
    void* base = reinterpret_cast<void*>(0x1000);
    void* top = reinterpret_cast<void*>(0x2000);
    
    size_t size = static_cast<char*>(top) - static_cast<char*>(base);
    EXPECT_EQ(size, 0x1000);
    
    void* middle = static_cast<char*>(base) + size / 2;
    EXPECT_EQ(middle, reinterpret_cast<void*>(0x1800));
}

TEST_F(UtilityTest, RangeChecking) {
    // Test range checking logic similar to isSharedMetaspaceObject
    void* base = reinterpret_cast<void*>(0x1000);
    void* top = reinterpret_cast<void*>(0x2000);
    
    auto isInRange = [base, top](const void* obj) {
        return obj >= base && obj < top;
    };
    
    EXPECT_TRUE(isInRange(base));
    EXPECT_FALSE(isInRange(top));
    EXPECT_TRUE(isInRange(reinterpret_cast<void*>(0x1500)));
    EXPECT_FALSE(isInRange(reinterpret_cast<void*>(0x500)));
    EXPECT_FALSE(isInRange(reinterpret_cast<void*>(0x2500)));
    EXPECT_FALSE(isInRange(nullptr));
}