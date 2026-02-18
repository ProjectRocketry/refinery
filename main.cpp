#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include "opcodes.h"
#include "decompile.h"
#include <bit>
#include "structs.h"

/* ============================================================
   Mnemonic canonicalization
   ============================================================ */
enum class MnemonicCase : int8_t {
    Invalid        = -1,
    Instruction    = 0,
    Directive      = 1,
    Reserved       = 2
};

struct CanonicalMnemonic {
    MnemonicCase type;
    std::string  normalized; // ALWAYS lowercase
};

/* ---- case helpers ---- */
static bool is_lower(char c) { return c >= 'a' && c <= 'z'; }
static bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }

static bool is_all_lower(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!is_lower(c)) return false;
    return true;
}

static bool is_all_upper(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!is_upper(c)) return false;
    return true;
}

static bool is_upper_camel(const std::string& s) {
    if (s.empty() || !is_upper(s[0])) return false;
    bool seenLower = false;
    for (char c : s) {
        if (is_lower(c)) seenLower = true;
        else if (!is_upper(c)) return false;
    }
    return seenLower;
}

static float uppercase_ratio(const std::string& s) {
    if (s.empty()) return 0.0f;
    size_t u = 0;
    for (char c : s) if (is_upper(c)) u++;
    return float(u) / float(s.size());
}

static std::string to_lower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        out.push_back(is_upper(c) ? (c - 'A' + 'a') : c);
    return out;
}

/* ---- canonical resolver ---- */
CanonicalMnemonic canonicalMnemonic(const std::string& raw) {
    if (is_all_upper(raw)) {
        return { MnemonicCase::Directive, to_lower(raw) };
    }

    if (is_upper_camel(raw)) {
        return { MnemonicCase::Reserved, to_lower(raw) };
    }

    if (is_all_lower(raw)) {
        return { MnemonicCase::Instruction, raw };
    }

    if (uppercase_ratio(raw) > 0.5f) {
        return { MnemonicCase::Invalid, {} };
    }

    return { MnemonicCase::Instruction, to_lower(raw) };
}

/* ============================================================
   Compiler helpers
   ============================================================ */
static std::string loadTextFile(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return {};

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    std::string text(sz, '\0');
    fread(text.data(), 1, sz, f);
    fclose(f);

    return text;
}

static hcFunc& beginFunction(
    std::vector<hcFunc>& funcs,
    char*& tokSave
) {
    funcs.emplace_back();
    hcFunc& f = funcs.back();

    char* ret = strtok_r(nullptr, " \t", &tokSave);
    if (ret && std::string(ret) == "entry") {
        f.isEntry = true;
        f.rettype=0xFF;
    } else {
        f.rettype=typecodeFor(ret);
        f.name = strtok_r(nullptr, " \t", &tokSave);
    }

    while (char* a = strtok_r(nullptr, " \t", &tokSave)) {
        if (std::string(a) == "{") break;
        f.argtypes.push_back(typecodeFor(a));
        f.argc++;
    }

    return f;
}

static Instr parseInstruction(
    const std::string& mnemonic,
    char*& tokSave,
    hcFunc& func
) {
    Instr i=makeInstr(opcodeFor(mnemonic));
    uint32_t idx = (uint32_t)func.code.size();

    if (mnemonic == "loadint") {
        i.operand = atoll(strtok_r(nullptr, " \t", &tokSave));
    } else if (mnemonic == "loadbool") {
        i.operand = atoi(strtok_r(nullptr, " \t", &tokSave));
    } else if (mnemonic == "call") {
        func.calls.emplace_back(idx, strtok_r(nullptr, " \t", &tokSave));
    } else if (mnemonic == "callnative") {
        func.callNatives.emplace_back(idx, strtok_r(nullptr, " \t", &tokSave));
    } else if (mnemonic == "loadarg"){
        i.operand = atoll(strtok_r(nullptr, " \t", &tokSave));
    } else if (mnemonic == "loadvar" || mnemonic == "savevar" || mnemonic == "incr" || mnemonic == "decr"){
        char* token=strtok_r(nullptr, " \t", &tokSave);
        uint64_t hash=hash64(token);
        i.operand = hash;
        func.varNames.emplace_back(hash,token);
    } else if (mnemonic == "branchlabel" || mnemonic == "jump" || mnemonic == "jumpiftrue" || mnemonic == "jumpiffalse"){
        char* token=strtok_r(nullptr, " \t", &tokSave);
        uint64_t hash=hash64(token);
        i.operand = hash;
        func.labelNames.emplace_back(hash,token);
    } else if (mnemonic == "loadfloat"){
        i.operand=std::bit_cast<uint64_t>(atof(strtok_r(nullptr, " \t", &tokSave)));
    }

    return i;
}

/* ============================================================
   COMPILER
   ============================================================ */
std::vector<hcFunc> compileHC(const char* filename) {
    std::vector<hcFunc> funcs;
    hcFunc* current = nullptr;

    std::string text = loadTextFile(filename);
    if (text.empty()) return funcs;

    char* buf = text.data();
    char* lineSave;

    for (char* line = strtok_r(buf, "\r\n", &lineSave);
         line;
         line = strtok_r(nullptr, "\r\n", &lineSave)) {

        char* tokSave;
        char* tok = strtok_r(line, " \t", &tokSave);
        if (!tok) continue;

        std::string raw(tok);

        if (raw == "func") {
            current = &beginFunction(funcs, tokSave);
            continue;
        }

        if (raw == "}") {
            current = nullptr;
            continue;
        }

        if (!current) continue;

        auto canon = canonicalMnemonic(raw);
        if (canon.type == MnemonicCase::Invalid) {
            fprintf(stderr, "Invalid mnemonic: %s\n", raw.c_str());
            exit(1);
        }
        switch (canon.type){
            case MnemonicCase::Instruction:
                current->code.push_back(parseInstruction(
                    canon.normalized,
                    tokSave,
                    *current
                ));
                break;
            default:
                break;
        }
    }

    return funcs;
}

/* ============================================================
   LINKER helpers
   ============================================================ */
static std::unordered_map<std::string, uint64_t>
buildFunctionIdTable(const std::vector<hcFunc>& funcs) {
    std::unordered_map<std::string, uint64_t> ids;
    for (auto& f : funcs)
        if (!f.isEntry)
            ids[f.name] = hash64(f.name);
    return ids;
}

static void resolveCalls(
    hcFunc& f,
    const std::unordered_map<std::string, uint64_t>& ids,
    DebugSection& debug
) {
    for (auto& c : f.calls)
        f.code[c.first].operand = ids.at(c.second);

    for (auto& c : f.callNatives){
        uint64_t hash=hash64(c.second);
        f.code[c.first].operand = hash;
        debug.callNativeNames.emplace_back(hash,c.second);
    }
}

struct BinaryWriter {
    std::vector<uint8_t> data;

    void write(const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        data.insert(data.end(), b, b + n);
    }

    template<typename T>
    void write(const T& v) {
        write(&v, sizeof(T));
    }
};
void writeString(BinaryWriter& out, std::string str){
    uint32_t len=str.size();
    out.write(len);
    out.write(str.data(),len);
}
std::vector<uint8_t> writeFuncDebugData(FuncDebugData data){
    BinaryWriter out;
    out.write(data.id);
    writeString(out,data.name);

    BinaryWriter varNamesOut;
    for (auto [id,name] : data.varNames){
        varNamesOut.write(id);
        writeString(varNamesOut, name);
    }
    out.write((uint32_t)data.varNames.size());
    out.write(varNamesOut.data.data(),(uint32_t)varNamesOut.data.size());

    BinaryWriter labelNamesOut;
    for (auto [id,name] : data.labelNames){
        labelNamesOut.write(id);
        writeString(labelNamesOut, name);
    }
    out.write((uint32_t)data.labelNames.size());
    out.write(labelNamesOut.data.data(),(uint32_t)labelNamesOut.data.size());
    
    out.write((uint32_t)data.argtypes.size());
    for (auto argtype : data.argtypes){
        out.write(argtype);
    }
    out.write(data.rettype);
    return out.data;
}
std::vector<uint8_t> writeDebugSection(DebugSection debug){
    BinaryWriter out;
    uint32_t funcLen=debug.functions.size();
    out.write(funcLen);

    for (FuncDebugData data : debug.functions){
        std::vector<uint8_t> buffer=writeFuncDebugData(data);
        uint32_t bufSize=buffer.size();
        out.write(buffer.data(),bufSize);
    }

    BinaryWriter callNamesOut;
    for (auto [id,name] : debug.callNames){
        callNamesOut.write(id);
        writeString(callNamesOut, name);
    }
    out.write((uint32_t)debug.callNames.size());
    out.write(callNamesOut.data.data(),(uint32_t)callNamesOut.data.size());
    BinaryWriter callNativeNamesOut;
    for (auto [id,name] : debug.callNativeNames){
        callNativeNamesOut.write(id);
        writeString(callNativeNamesOut, name);
    }
    out.write((uint32_t)debug.callNativeNames.size());
    out.write(callNativeNamesOut.data.data(),(uint32_t)callNativeNamesOut.data.size());
    return out.data;
}
/* ============================================================
   LINKER
   ============================================================ */
std::vector<uint8_t> link(const std::vector<hcFunc>& funcs, bool debugFlag) {
    auto ids = buildFunctionIdTable(funcs);
    BinaryWriter out;
    DebugSection debug;
    for (auto [name,id] : ids){
        debug.callNames.emplace_back(id,name);
    }
    out.write("PROP", 4);
    uint32_t count = funcs.size();
    out.write(count);

    for (auto f : funcs) {  // copy is intentional
        FuncDebugData fnDebug;
        uint64_t id = f.isEntry
            ? ENTRY_FUNCTION_ID
            : ids.at(f.name);
        fnDebug.id=id;
        fnDebug.name=f.name;
        fnDebug.varNames=f.varNames;
        fnDebug.labelNames=f.labelNames;
        fnDebug.argtypes=f.argtypes;
        fnDebug.rettype=f.rettype;
        out.write(id);
        out.write(f.argc);

        resolveCalls(f, ids, debug);

        // HARDEN: enforce clean instruction fields
        for (auto& ins : f.code) {
            ins.extra    = 0;
            ins.reserved = 0;
            ins.flags    = 0;
        }

        uint32_t codeSize = f.code.size() * sizeof(Instr);
        out.write(codeSize);
        out.write(f.code.data(), codeSize);
        debug.functions.emplace_back(fnDebug);
    }
    if (debugFlag){
        std::vector<uint8_t> debugSectionBuffer=writeDebugSection(debug);
        out.write((uint32_t)debugSectionBuffer.size());
        out.write(debugSectionBuffer.data(),(uint32_t)debugSectionBuffer.size());
    }
    return out.data;
}

/* ============================================================
   MAIN
   ============================================================ */
int main(int argc, char** argv) {
    if (argc < 3) return 255;

    if (std::string(argv[1]) == "--disassemble") {
        FILE* in = fopen(argv[2], "rb");
        if (!in) return 1;

        fseek(in, 0, SEEK_END);
        long sz = ftell(in);
        rewind(in);

        std::vector<uint8_t> bin(sz);
        fread(bin.data(), 1, sz, in);
        fclose(in);

        FILE* out = fopen(argv[3], "w");
        decompilePROP(bin, out, DecompileMode::ToFile);
        fclose(out);
        return 0;
    }

    if (std::string(argv[1]) == "--log") {
        FILE* in = fopen(argv[2], "rb");
        if (!in) return 1;

        fseek(in, 0, SEEK_END);
        long sz = ftell(in);
        rewind(in);

        std::vector<uint8_t> bin(sz);
        fread(bin.data(), 1, sz, in);
        fclose(in);

        decompilePROP(bin, stdout, DecompileMode::Log);
        return 0;
    }

    auto obj = compileHC(argv[1]);
    auto bin = link(obj, true);

    FILE* outfile = fopen(argv[2], "wb");
    if (!outfile) {
        perror("fopen");
        return 1;
    }

    fwrite(bin.data(), 1, bin.size(), outfile);
    fclose(outfile);

    return 0;
}
