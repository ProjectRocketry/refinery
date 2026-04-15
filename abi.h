#pragma once
#include <nlohmann/json.hpp>
#include "../libpropfile/prop_hc_interface.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

/* ============================================================
   ValueType <-> string conversion
   ============================================================ */

inline std::string valueTypeToString(ValueType t) {
    switch (t) {
        case ValueType::Null:   return "null";
        case ValueType::String: return "string";
        case ValueType::Int:    return "int";
        case ValueType::Float:  return "float";
        case ValueType::Bool:   return "bool";
        case ValueType::UInt:   return "uint";
        case ValueType::Entry:  return "entry";
        case ValueType::Any:    return "any";
        default: throw std::runtime_error("Unknown ValueType");
    }
}

inline ValueType stringToValueType(const std::string& s) {
    if (s == "null")   return ValueType::Null;
    if (s == "string") return ValueType::String;
    if (s == "int")    return ValueType::Int;
    if (s == "float")  return ValueType::Float;
    if (s == "bool")   return ValueType::Bool;
    if (s == "uint")   return ValueType::UInt;
    if (s == "entry")  return ValueType::Entry;
    if (s == "void")   return ValueType::Null; // for ABI convenience
    if (s == "any")    return ValueType::Any;
    throw std::runtime_error("Unknown ValueType string: " + s);
}

/* ============================================================
   ABIEntry serialization
   ============================================================ */

inline void to_json(json& j, const ABIEntry& e) {
    j = json{
        {"name", e.name},
        {"returnType", valueTypeToString(e.returnType)},
        {"argTypes", json::array()}
    };

    for (auto t : e.argTypes) {
        j["argTypes"].push_back(valueTypeToString(t));
    }
}

inline void from_json(const json& j, ABIEntry& e) {
    e.name = j.at("name").get<std::string>();

    std::string retStr = j.at("returnType").get<std::string>();
    e.returnType = stringToValueType(retStr);

    e.argTypes.clear();
    for (const auto& arg : j.at("argTypes")) {
        e.argTypes.push_back(stringToValueType(arg.get<std::string>()));
    }
}

/* ============================================================
   ABITable serialization
   ============================================================ */

inline void to_json(json& j, const ABITable& t) {
    j = json::array();
    for (const auto& e : t) {
        j.push_back(e);
    }
}

inline void from_json(const json& j, ABITable& t) {
    t.clear();
    for (const auto& entry : j) {
        t.push_back(entry.get<ABIEntry>());
    }
}

/* ============================================================
   Load ABI from file
   ============================================================ */

inline ABITable getABITable(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open())
        throw std::runtime_error("Failed to open ABI file: " + filepath);

    json abiJson;
    f >> abiJson;

    return abiJson.get<ABITable>();
}