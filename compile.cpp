#include "mnemonic.h"
#include "../libpropfile/opcodes.h"
#include <cstring>
#include <stdexcept>
#include <charconv>
#include "refinery.h"
#include <iostream>
struct Tokenizer {
    std::string remaining; // owns the string now
};

std::optional<std::string> tokenize(std::optional<std::string> input, Tokenizer& state, const char* delims) {
    if (input) {
        state.remaining = *input;
    }
    while (!state.remaining.empty()) {
        size_t start = 0;
        while (start < state.remaining.size() && strchr(delims, state.remaining[start])) start++;
        if (start == state.remaining.size()) {
            state.remaining.clear();
            return std::nullopt;
        }
        state.remaining.erase(0, start);
        size_t end = 0;
        while (end < state.remaining.size() && !strchr(delims, state.remaining[end])) end++;
        std::string token = state.remaining.substr(0, end);
        state.remaining.erase(0, end);
        if (!token.empty()) {
            return token;
        }
    }
    return std::nullopt;
}
std::optional<std::string> getNextLine(std::optional<std::string> input, Tokenizer& state){
    return tokenize(input, state, "\r\n");
}

std::optional<std::string> getNextWord(std::optional<std::string> input, Tokenizer& state){
    return tokenize(input, state, " \t");
}
void throwIfEmpty(std::optional<std::string> str, std::string errorText){
	if (!str) throw std::runtime_error(errorText);
}
template<typename T>
void strToT(const std::string& str, T& t){
	auto result = std::from_chars(str.data(),str.data()+str.size(),t);
	if (result.ec != std::errc() || result.ptr != str.data() + str.size()){
		throw std::runtime_error("Invalid number");
	}
}
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
ValueType getValueType(const std::string& typeString,bool allowEntry){
	if (typeString=="entry"){
		if (!allowEntry){
			throw std::runtime_error("Type entry cannot be an argument");
		}
		return ValueType::Entry;
	} else if (typeString=="string"){
		return ValueType::String;
	} else if (typeString=="int"){
		return ValueType::Int;
	} else if (typeString=="float"){
		return ValueType::Float;
	} else if (typeString=="uint"){
		return ValueType::UInt;
	} else if (typeString=="void" || typeString=="null"){
		return ValueType::Null;
	} else if (typeString=="bool"){
		return ValueType::Bool;
	} else if (typeString=="any"){
		return ValueType::Any;
	}
	throw std::runtime_error("Unknown type "+typeString);
}
void handleFunctionDecl(PropellantObject& propObj,Tokenizer& tokSave,std::optional<UnlinkedPropFunction>& currentFN){//returns if should error
	try {
		UnlinkedPropFunction func;
		std::optional<std::string> returnTypeString=getNextWord(std::nullopt, tokSave);
		if (!returnTypeString) throw std::runtime_error("Missing field in declaration");
		ValueType retType=getValueType(*returnTypeString,true);//allow entry keyword
		func.returnType=retType;
		if (retType!=ValueType::Entry){
			std::optional<std::string> fnName=getNextWord(std::nullopt, tokSave);//this is next word
			if (!fnName) throw std::runtime_error("Missing field in declaration");
			func.name=*fnName;
		}
		std::optional<std::string> argT=getNextWord(std::nullopt,tokSave);
		if (!argT) throw std::runtime_error("Missing field in declaration");
		while (argT!="{"){
			ValueType argType=getValueType(*argT,false);
			func.args.push_back(argType);
			argT=getNextWord(std::nullopt,tokSave);
			if (!argT) throw std::runtime_error("Missing field in declaration");
		}
		currentFN=func;
		return;
	} catch (std::runtime_error& error){
		throw std::runtime_error("Error in function declaration: "+std::string(error.what()));
	}
}
Value getOperandForInstruction(std::string& instr,Tokenizer& save){
	Value v;
	OperandType type=operandTypeFor(opcodeFor(instr));
	if (type==OperandType::Int){
		v.type=ValueType::Int;
		std::optional<std::string> word=getNextWord(std::nullopt,save);
		if (!word) throw std::runtime_error("Invalid loadint, missing operand");
		int64_t temp;
		strToT(*word,temp);
		v.value=temp;
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		return v;
	} else if (type==OperandType::String){
		v.type=ValueType::String;
		std::optional<std::string> word=getNextWord(std::nullopt,save);
		if (!word) throw std::runtime_error("Invalid loadstring, missing operand");
		std::string raw=*word;
		v.value=raw;
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		return v;
	} else if (type==OperandType::StringLiteral){
		v.type=ValueType::String;
		std::optional<std::string> word=getNextWord(std::nullopt,save);
		if (!word) throw std::runtime_error("Invalid loadstring, missing operand");
		std::string raw=*word;
		if (raw.front()!='\"' && raw.front()!='\''){
			throw std::runtime_error("Unquoted string");
		}
		for (;;){
			std::optional<std::string> nextword=getNextWord(std::nullopt,save);
			if (nextword){
				raw += " ";
				raw += *nextword;
			} else {
				break;
			}
		}
		if (raw.back()!='\"' && raw.back()!='\''){
			throw std::runtime_error("Unterminated string");
		}
		std::string contents=raw.substr(1, raw.size()-2);
		v.value=contents;
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		return v;
	} else if (type==OperandType::Float){
		v.type=ValueType::Float;
		std::optional<std::string> word=getNextWord(std::nullopt,save);
		if (!word) throw std::runtime_error("Invalid loadfloat, missing operand");
		double temp;
		strToT(*word,temp);
		v.value=temp;
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		return v;
	} else if (type==OperandType::Bool){
		v.type=ValueType::Bool;
		std::optional<std::string> word=getNextWord(std::nullopt,save);
		if (!word) throw std::runtime_error("Invalid loadbool, missing operand");
		if (to_lower(*word)=="true") v.value=true;
		else if (to_lower(*word)=="false") v.value=false;
		else throw std::runtime_error("Invalid loadbool, can only be 'true' or 'false', got "+to_lower(*word));
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		return v;
	} else if (type==OperandType::UInt){
		v.type=ValueType::UInt;
		std::optional<std::string> word=getNextWord(std::nullopt,save);
		if (!word) throw std::runtime_error("Invalid loadint, missing operand");
		uint64_t temp;
		strToT(*word,temp);
		v.value=temp;
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		return v;
	} else if (type==OperandType::None){
		v.type=ValueType::Null;
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		return v;
	}
	throw std::runtime_error("Instruction missing operand type");
}
void handleInstruction(std::string& normalizedInstr,Tokenizer& save, std::vector<RawInstruction>& code){
	code.emplace_back();
	RawInstruction& instr=code.back();
	instr.opcode=opcodeFor(normalizedInstr);
	instr.operand=getOperandForInstruction(normalizedInstr,save);
}
void handleDirective(PropellantObject& prop, std::string& normalizedDirective,Tokenizer& save){
	if (normalizedDirective=="embeddedfilename"){
		std::optional<std::string> filename=getNextWord(std::nullopt,save);
		if (!filename) throw std::runtime_error("Not enough operands");
		if (getNextWord(std::nullopt,save)) throw std::runtime_error("Too many operands");
		prop.dirs.wantedFiles.push_back(*filename);
	}
}
void handleMacro(PropellantObject& prop, std::string& normalizedDirective, Tokenizer& save, std::vector<RawInstruction>& code){
	if (normalizedDirective=="embeddedfilename"){
		auto filename = getNextWord(std::nullopt,save);
		if (!filename) throw std::runtime_error("Not enough operands");
		Tokenizer dirSave;
		std::string dir="embeddedfilename "+*filename;
		std::optional<std::string> dirWord=getNextWord(dir,dirSave);
		if (!dirWord) throw std::runtime_error("Internal error, code EINVALIDDIRWORD");
		handleDirective(prop,*dirWord,dirSave);
		Tokenizer instrSave;
		std::string instr="loadstring "+*filename;
		std::optional<std::string> instrWord=getNextWord(instr,instrSave);
		if (!dirWord) throw std::runtime_error("Internal error, code EINVALIDINSTRWORD");
		handleInstruction(*instrWord,instrSave,code);
	}
}
void handleLine(PropellantObject& propObj, std::string line, std::optional<UnlinkedPropFunction>& currentFN, int& line_num){
	Tokenizer wordSave;
    std::optional<std::string> wordMaybe = getNextWord(line, wordSave);
    if (!wordMaybe) return;
    std::string word=*wordMaybe;
    try {
	    if (word=="func"){
	    	//function decl
	    	if (currentFN){
	    		throw std::runtime_error("Cannot nest functions, invalid function declaration");
	    	}
	    	handleFunctionDecl(propObj,wordSave,currentFN);
	    	return;
	    }
	    if (word=="}"){
	    	if (!currentFN){
	    		throw std::runtime_error("No function to close");
	    	}
	    	propObj.functions.push_back(*currentFN);
	    	currentFN=std::nullopt;
	    	return;
	    }
	    Mnemonic m=toMnemonic(word);
	    if (currentFN){
	    	UnlinkedPropFunction& fn=*currentFN;
	    	switch (m.type){
	    		case MnemonicType::Instruction:
	    			handleInstruction(m.normalized,wordSave,fn.code);
	    			break;
	    		case MnemonicType::Directive:
	    			handleDirective(propObj,m.normalized,wordSave);
	    			break;
	    		case MnemonicType::Macro:
	    			handleMacro(propObj,m.normalized,wordSave,fn.code);
	    			break;
	    		case MnemonicType::Invalid:
	    			throw std::runtime_error("Invalid mnemonic");
	    	}
	    } else {
	    	switch (m.type){
	    		case MnemonicType::Directive:
	    			handleDirective(propObj,m.normalized,wordSave);
	    			break;
	    		default:
	    			throw std::runtime_error("Invalid mnemonic");
	    	}
	    }
	    return;
	} catch (std::runtime_error& error){
		throw std::runtime_error(std::string(error.what())+" at line "+std::to_string(line_num));
	}
}
PropellantObject compileFile(std::string filename){
	PropellantObject propObj;
	std::optional<UnlinkedPropFunction> currentFN=std::nullopt;
	std::string text = loadTextFile(filename.c_str());
	if (text.empty()){
		throw std::runtime_error("File empty");
	}
    Tokenizer lineSave;
    int line_num=1;
    for (std::optional<std::string> line = getNextLine(text, lineSave);
         line;
         line = getNextLine(std::nullopt, lineSave)){
    	handleLine(propObj,*line,currentFN,line_num);
    	line_num++;
    }
    return propObj;
}