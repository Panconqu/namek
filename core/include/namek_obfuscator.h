#ifndef NAMEK_OBFUSCATOR_H
#define NAMEK_OBFUSCATOR_H

#include <string>
#include <vector>

namespace namek {

enum class ObfuscationLevel {
    HIGH_LEVEL,   // AST transformation, string XOR encryption, dead code injection
    VM_LEVEL,     // Virtual Machine Bytecode Compiler + Embedded Runtime Interpreter
    NATIVE_COMPILE// C++ Wrapper Compilation + Binary Stripping (-s -O3)
};

class Obfuscator {
public:
    // Core Obfuscation Methods
    static std::string obfuscate_high_level(const std::string& source_code, const std::string& language = "python");
    // Nested-layered VM obfuscation: layers >= 1 (default 5 = "Nivel 5")
    static std::string obfuscate_vm(const std::string& source_code, const std::string& language = "python", int layers = 5);
    static bool compile_native(const std::string& source_code, const std::string& output_binary_path, const std::string& language = "python");

    // Dynamic MV Deobfuscator & Runtime Unpacker Engine (unwraps ALL layers)
    static std::string deobfuscate_vm(const std::string& obf_vm_code);
    static std::string deobfuscate_high_level(const std::string& obf_code);

    // Anti-Analysis & Anti-Debugging Guard
    static std::string inject_anti_debug_guard(const std::string& code);

    // Helpers
    static std::string xor_encrypt(const std::string& input, char key = 0x5A);
    static std::string generate_random_identifier(size_t length = 12);
};

} // namespace namek

#endif // NAMEK_OBFUSCATOR_H
