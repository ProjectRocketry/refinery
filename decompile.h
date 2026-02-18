#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>

// Disassembler modes
enum class DecompileMode {
    ToFile,   // emit HC-like text
    Log       // verbose logging
};

// Disassemble a PROP binary buffer
//  - mode == ToFile : writes HC text to `out`
//  - mode == Log    : writes human-readable logs to `out`
bool decompilePROP(const std::vector<uint8_t>& bin,
                    FILE* out,
                    DecompileMode mode);
