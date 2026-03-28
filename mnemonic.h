#include <cstdint>
#include <string>
enum class MnemonicType : int8_t {
    Invalid        = -1,
    Instruction    = 0,
    Directive      = 1,
    Macro          = 2
};
struct Mnemonic {
    MnemonicType type;
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
Mnemonic toMnemonic(const std::string& raw) {
    if (is_all_upper(raw)) {
        return { MnemonicType::Directive, to_lower(raw) };
    }

    if (is_upper_camel(raw)) {
        return { MnemonicType::Macro, to_lower(raw) };
    }

    if (is_all_lower(raw)) {
        return { MnemonicType::Instruction, raw };
    }

    if (uppercase_ratio(raw) > 0.5f) {
        return { MnemonicType::Invalid, {} };
    }

    return { MnemonicType::Instruction, to_lower(raw) };
}