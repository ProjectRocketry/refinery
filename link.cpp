#include "refinery.h"
#include "../libpropfile/opcodes.h"
#include "abi.h"
#include <bit>
#include <stdexcept>
#include <unordered_set>
[[noreturn]] inline void INTERNAL_ERROR(const char* code) {
    throw std::runtime_error(std::string("Internal linker error, code ") + code);
}
std::vector<std::string> REF_ERRS;
std::vector<std::string> VAL_ERRS;
inline void throwErrors(bool didValidation){
    std::string errs;
    bool errored=false;
    if (!REF_ERRS.empty()){
        errored=true;
        for (std::string& ref_err : REF_ERRS){
            errs+=ref_err;
            errs+="\n";
        }
    }
    if (!VAL_ERRS.empty()){
        errored=true;
        for (std::string& val_err : VAL_ERRS){
            errs+=val_err;
            errs+="\n";
        }
    }
    if (errored){
        if (!didValidation) errs+="Skipped validation due to previous errors\n";
        errs+="Problems detected, stopping";
        errs+="\n";
        std::cerr << errs;
        exit(1);
    }
    return;
}
inline void REFERENCE_ERROR(const std::string& msg) {
    REF_ERRS.push_back(std::string("Reference error: ") + msg);
}
inline void VALIDATION_ERROR(const std::string& msg) {
    VAL_ERRS.push_back(std::string("Error in validation: ") + msg);
}
uint64_t hash64(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}
constexpr uint64_t ERROR_ID=0xFFFF'FFFF'FFFF'FFFF;
uint64_t resolveCallNative(const std::string& value, LinkFnContext& ctx){
    ABITable& natives=ctx.ctx.callNativeTable;
    for (ABIEntry& callNative : natives){
        if (value==callNative.name) return hash64(value);
    }
    REFERENCE_ERROR("Tried to call native function "+value+" but it does not exist");
    return ERROR_ID;
}
StrippedPropellant strip(const LinkedPropellant& prop){
    StrippedPropellant stripped;
    stripped.stringTable=prop.stringTable;
    // stripped.fs=prop.fs;
    stripped.functions=prop.functions;
    return stripped;
}
uint64_t handleStringOperand(const Opcode opcode, const std::string& value, LinkFnContext& ctx){
    OperandType expected=operandTypeFor(static_cast<uint8_t>(opcode));
    if (expected!=OperandType::String) INTERNAL_ERROR("EINVALIDSTRINGOPERAND");
    uint64_t id=hash64(value);
    switch (opcode){
        case Opcode::SaveVar:
            ctx.debug.varNames[id]=value;
            return id;
        case Opcode::LoadVar:
        case Opcode::Incr:
        case Opcode::Decr:
            if (!ctx.debug.varNames.contains(id)){
                REFERENCE_ERROR("Tried to use variable "+value+" before it was set to anything");
                return ERROR_ID;
            }
            return id;
        case Opcode::Jump:
        case Opcode::JumpIfFalse:
        case Opcode::JumpIfTrue:
            ctx.usedLabels.insert(value);
            return id;
        case Opcode::JumpLabel:
            if (ctx.debug.labelNames.contains(id)){
                REFERENCE_ERROR("Duplicate label " + value);
                return ERROR_ID;
            }
            ctx.debug.labelNames[id]=value;
            return id;
        case Opcode::Call:
            if (!ctx.ctx.functionIdTable.contains(value)){
                REFERENCE_ERROR("Tried to call function "+value+" but it is not defined anywhere");
                return ERROR_ID;
            }
            id=ctx.ctx.functionIdTable.at(value);
            ctx.debug.callNames[id]=value;
            return id;
        case Opcode::CallNative:
            id=resolveCallNative(value,ctx);//TODO: write
            ctx.debug.callNativeNames[id] = value;
            return id;
        default:
            ctx.ctx.stringTable[id]=value;
            return id;
    }
}
uint64_t handleIntOperand(const Opcode opcode, const int64_t value, LinkFnContext& ctx){
    OperandType expected=operandTypeFor(static_cast<uint8_t>(opcode));
    if (expected!=OperandType::Int) INTERNAL_ERROR("EINVALIDINTOPERAND");   
    switch (opcode){
        // overrides later
        default:
            return static_cast<uint64_t>(value);
    }
}
uint64_t handleNullOperand(const Opcode opcode, LinkFnContext& ctx){
    OperandType expected=operandTypeFor(static_cast<uint8_t>(opcode));
    if (expected!=OperandType::None) INTERNAL_ERROR("EINVALIDNULLOPERAND");
    switch (opcode){
        // overrides later
        default:
            return 0x0000'0000'0000'0000;
    }
}
uint64_t handleBoolOperand(const Opcode opcode, const bool value, LinkFnContext& ctx){
    OperandType expected=operandTypeFor(static_cast<uint8_t>(opcode));
    if (expected!=OperandType::Bool) INTERNAL_ERROR("EINVALIDBOOLOPERAND");
    switch (opcode){
        // overrides later
        default:
            return value?1ull:0ull;
    }
}
uint64_t handleFloatOperand(const Opcode opcode, const double value, LinkFnContext& ctx){
    switch (opcode){
        //overrides later
        default:
            OperandType expected=operandTypeFor(static_cast<uint8_t>(opcode));
            if (expected!=OperandType::Float) INTERNAL_ERROR("EINVALIDFLOATOPERAND");
            return std::bit_cast<uint64_t>(value);
    }
}
uint64_t handleUIntOperand(const Opcode opcode, const uint64_t value, LinkFnContext& ctx){
    OperandType expected=operandTypeFor(static_cast<uint8_t>(opcode));
    if (expected!=OperandType::UInt) INTERNAL_ERROR("EINVALIDUINTOPERAND");
    switch (opcode){
        //overrides later
        default:
            return value;
    }
}
uint64_t resolveOperandFromValue(const Opcode opcode, const Value& value, LinkFnContext& ctx){
    try {
        switch (value.type){
            case ValueType::Null:
                return handleNullOperand(opcode,ctx);
            case ValueType::Bool:
                return handleBoolOperand(opcode,std::get<bool>(value.value),ctx);
            case ValueType::Float:
                return handleFloatOperand(opcode,std::get<double>(value.value),ctx);
            case ValueType::String:
                return handleStringOperand(opcode, std::get<std::string>(value.value),ctx);
            case ValueType::Int:
                return handleIntOperand(opcode, std::get<int64_t>(value.value),ctx);
            case ValueType::UInt:
                return handleUIntOperand(opcode,std::get<uint64_t>(value.value),ctx);
            default:
                INTERNAL_ERROR("EINVALIDVALUETYPE");
        }
    } catch (std::bad_variant_access& err){
        INTERNAL_ERROR("EVALUETYPEMISMATCH");
    }
}
LinkedInstruction linkInstruction(const RawInstruction& raw, LinkFnContext& ctx){
    LinkedInstruction linked;
    linked.opcode=raw.opcode;
    //these are reserved for later, zero them for now
    linked.flags=0x00;
    linked.reserved=0x0000;
    linked.extra=0x00000000;
    linked.operand=resolveOperandFromValue(static_cast<Opcode>(raw.opcode),raw.operand,ctx);
    return linked;
}
LinkedPropFunction linkFunction(const UnlinkedPropFunction& fn, LinkContext& ctx, uint64_t& id){
    LinkedPropFunction linked;
    LinkFnContext fnCtx{{},{},ctx};
    if (fn.returnType==ValueType::Entry){
        if (ctx.entryFuncFound) REFERENCE_ERROR("Multiple entrypoints found");
        id=ENTRY_FUNCTION_ID;
        ctx.entryFuncFound=true;
    } else {
        if (!ctx.functionIdTable.contains(fn.name)) INTERNAL_ERROR("EUNINDEXEDFUNCTION");
        id=ctx.functionIdTable.at(fn.name);
        fnCtx.debug.functionName = fn.name;
    }
    linked.returnType=fn.returnType;
    linked.args=fn.args;
    for (const RawInstruction& instr : fn.code){
        linked.code.push_back(linkInstruction(instr,fnCtx));
    }
    for (const std::string& label : fnCtx.usedLabels){
        uint64_t id=hash64(label);
        if (!fnCtx.debug.labelNames.contains(id) || fnCtx.debug.labelNames.at(id)!=label){
            REFERENCE_ERROR("Undefined label "+label);
        }
    }
    ctx.symbols.emplace(id,fnCtx.debug);
    return linked;
}
void createFunctionIdTable(const UnlinkedPropellant& prop,LinkContext& ctx){
    std::unordered_set<std::string> fdata;
    for (const PropellantObject& pobj : prop.objects){
        for (const UnlinkedPropFunction& fn : pobj.functions){
            if (fn.returnType==ValueType::Entry) continue;//entry is unique and handled in linkFunction, and needs to not be able to be called, hence not in table
            if (fdata.contains(fn.name)){
                REFERENCE_ERROR("Multiple functions with name "+fn.name+" found");
            } else {
                fdata.insert(fn.name);
            }
        }
    }
    for (const std::string& data : fdata){
        ctx.functionIdTable[data]=hash64(data);
    }
}
bool isNumber(ValueType t) {
    return t == ValueType::Int ||
           t == ValueType::UInt ||
           t == ValueType::Float;
}
bool isCompatible(ValueType actual, ValueType expected) {
    if (expected == ValueType::Any || actual == ValueType::Any)
        return true;

    if (expected == ValueType::Number)
        return isNumber(actual);
    if (actual == ValueType::Number)
        return isNumber(expected);
    return actual == expected;
}
void underflowed(uint64_t instructionNum, const std::string& extra){
    throw std::underflow_error(extra+" at "+std::to_string(instructionNum));
}
void handleInstructionForStackValidation(const OpcodeEntry& op, std::vector<ValueType>& stack, uint64_t instructionNum){
    for (size_t it=op.popTypes.size();it-->0;){
        ValueType v=op.popTypes[it];
        if (stack.empty()) underflowed(instructionNum,"Stack empty "+std::to_string(it)+" for instruction "+op.primary);
        if (!isCompatible(stack.back(),v)){
            underflowed(instructionNum,"Missing item on stack for argument "+std::to_string(it)+" for instruction "+op.primary);
        } else {
            stack.pop_back();
        }
    }
    for (ValueType v : op.pushTypes){
        stack.push_back(v);
    }
}
void validateStackForFunction(const LinkedPropellant& linked,uint64_t id, const LinkContext& ctx){
    std::vector<ValueType> stack;
    if (!linked.functions.contains(id)) INTERNAL_ERROR("EMISSINGFNNOVALID");
    if (!linked.symbols.contains(id)) INTERNAL_ERROR("EMISSINGSYMNOVALID");
    const LinkedPropFunction& fn=linked.functions.at(id);
    const DebugData& fnsym=linked.symbols.at(id);
    uint64_t instructionNum=1;
    for (const LinkedInstruction& i : fn.code){
        Opcode op=static_cast<Opcode>(i.opcode);
        OpcodeEntry opEn=opcodeEntryFor(op);

        if (!opEn.pushTypes.empty() && opEn.pushTypes[0]==ValueType::Null){
            //special cases
            switch (op){
                case Opcode::Call: {
                    if (!linked.functions.contains(i.operand)) INTERNAL_ERROR("EFOUNDCALLNOTFOUND");
                    if (!linked.symbols.contains(i.operand)) INTERNAL_ERROR("EFOUNDCALLNOSYMBOLS");
                    const LinkedPropFunction& callfn=linked.functions.at(i.operand); 
                    const DebugData& callfnsym=linked.symbols.at(i.operand);
                    for (size_t it=callfn.args.size();it-->0;){
                        const ValueType& v=callfn.args[it];
                        if (stack.empty()){
                            underflowed(instructionNum,"Missing item on stack for argument "+std::to_string(it)+" for call to "+callfnsym.functionName);
                        }
                        // strict validation, might introduce rewriting and warn later...
                        else if (!isCompatible(stack.back(),v)){//specifically allow an any type to be safe since we cant know otherwise
                            underflowed(instructionNum,"Argument "+std::to_string(it)+" with type "+ValueTypeAsString.at(stack.back())+" for call to function "+callfnsym.functionName+" was not the correct type, expected "+ValueTypeAsString.at(v));
                        } else {
                            stack.pop_back();
                        }
                    }
                    if (callfn.returnType != ValueType::Null) {
                        stack.push_back(callfn.returnType);
                    }
                    break;
                }
                case Opcode::CallNative: {
                    bool callFound=false;
                    for (const ABIEntry& e : ctx.callNativeTable){
                        if (hash64(e.name)==i.operand){
                            for (size_t it=e.argTypes.size();it-->0;){
                                const ValueType& v=e.argTypes[it];
                                if (stack.empty()){
                                    underflowed(instructionNum,"Missing item on stack for argument "+std::to_string(it)+" for callnative to "+e.name);
                                }
                                // strict validation, might introduce rewriting and warn later...
                                else if (!isCompatible(stack.back(),v)){//specifically allow an any type to be safe since we cant know otherwise
                                    underflowed(instructionNum,"Argument "+std::to_string(it)+" with type "+ValueTypeAsString.at(stack.back())+" for callnative to function "+e.name+" was not the correct type, expected "+ValueTypeAsString.at(v));
                                } else {
                                    stack.pop_back();
                                }
                            }
                            if (e.returnType != ValueType::Null) {
                                stack.push_back(e.returnType);
                            }
                            callFound=true;
                            break;
                        }
                    }
                    if (!callFound) INTERNAL_ERROR("EFOUNDCALLNATIVENOTFOUND");
                    break;
                }
                case Opcode::Exit:
                    return;
                case Opcode::Return:
                    if (fn.returnType==ValueType::Null){
                        if (!stack.empty()){
                            VALIDATION_ERROR("Stack overflow detected: return type void cannot return a value in function "+fnsym.functionName);
                        }
                    } else {
                        if (stack.empty()){
                            underflowed(instructionNum,"No value to return");
                        } else if (!isCompatible(stack.back(),fn.returnType)){//specifically allow an any type to be safe since we cant know otherwise
                            VALIDATION_ERROR("Top value of stack is not the correct type to return for function "+fnsym.functionName);
                        }
                        if (stack.size()>1){
                            VALIDATION_ERROR("Stack overflow detected: too many items remain on stack after return for function "+fnsym.functionName);
                        }
                    }
                    return;
                default:
                    INTERNAL_ERROR("EUNDEFINEDSPECIAL");
            }
        } else {
            handleInstructionForStackValidation(opEn,stack,instructionNum);
        }
        instructionNum++;
    }
    VALIDATION_ERROR(fnsym.functionName+" failed to return or exit");
}
void doValidation(const LinkedPropellant& prop, const LinkContext& ctx){
    for (const auto& [id,fn]: prop.functions){
        if (!prop.symbols.contains(id)) INTERNAL_ERROR("EMISSINGSYMPREVALID");
        const DebugData& fnsym=prop.symbols.at(id);
        try {
            validateStackForFunction(prop,id,ctx);
        } catch (std::underflow_error& e){
            VALIDATION_ERROR("Stack underflow detected: " + std::string(e.what()) + " in function "+fnsym.functionName);
        }
    }
}
LinkedPropellant link(const UnlinkedPropellant& prop, std::string& abiFilepath){
    LinkedPropellant linked;
    LinkContext ctx;
    ctx.callNativeTable=getABITable(abiFilepath);
    createFunctionIdTable(prop,ctx);
    for (const PropellantObject& pobj : prop.objects){
        for (const UnlinkedPropFunction& fn : pobj.functions){
            uint64_t id=0;
            LinkedPropFunction lfn=linkFunction(fn,ctx,id);
            linked.functions.emplace(id,lfn);
        }
    }
    linked.stringTable=ctx.stringTable;
    linked.symbols=ctx.symbols;
    if (!ctx.entryFuncFound) REFERENCE_ERROR("Entrypoint missing");
    throwErrors(false);
    doValidation(linked,ctx);
    throwErrors(true);
    return linked;
}