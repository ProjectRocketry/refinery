#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
/* ============================================================
   Instruction format (FIXED 16 BYTES)
   ============================================================ */
struct Instr {
    uint64_t operand;    // 8
    uint32_t extra;      // 4
    uint16_t reserved;   // 2
    uint8_t  opcode;     // 1
    uint8_t  flags;      // 1
};
static_assert(sizeof(Instr) == 16, "Instr must be 16 bytes");
inline Instr makeInstr(uint8_t opcode) {
    Instr i;
    std::memset(&i, 0, sizeof(Instr));
    i.opcode = opcode;
    return i;
}
/* ============================================================
   Compiler output: one function
   ============================================================ */
struct hcFunc {
    bool isEntry = false;
    std::string name;
    uint64_t id;
    uint32_t argc = 0;
    uint8_t rettype;
    std::vector<Instr> code;
    std::vector<std::pair<uint64_t, std::string>> varNames;
    std::vector<std::pair<uint64_t, std::string>> labelNames;
    std::vector<std::pair<uint32_t, std::string>> calls;
    std::vector<std::pair<uint32_t, std::string>> callNatives;
    std::vector<std::pair<uint32_t, std::string>> callExterns;
    std::vector<uint8_t> argtypes;
};
struct FuncDebugData {
    uint64_t id;
    uint8_t rettype;
    std::string name;
    std::vector<std::pair<uint64_t, std::string>> varNames;
    std::vector<std::pair<uint64_t, std::string>> labelNames;
    std::vector<uint8_t> argtypes;
};
struct DebugSection {
    std::vector<FuncDebugData> functions;
    std::vector<std::pair<uint64_t, std::string>> callNames;
    std::vector<std::pair<uint64_t, std::string>> callNativeNames;
};
struct FullProp {
    std::vector<hcFunc> functions;
    DebugSection debug;
};
/* ============================================================
   Constants
   ============================================================ */
static constexpr uint64_t ENTRY_FUNCTION_ID = 0xFFFFFFFFFFFFFFFFull;

/* ============================================================
   Hash (linker/runtime side)
   ============================================================ */
inline uint64_t hash64(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}