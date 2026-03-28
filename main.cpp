#include <vector>
#include "refinery.h"
#include "../libpropfile/propfile.h"
#include <fstream>
int main(int argc, char const *argv[])
{
	std::vector<std::string> args(argv + 1, argv + argc);
	if (args.size()<2 || !args[0].ends_with(".hc") || !args[1].ends_with(".prop")){
		fprintf(stderr, "Usage: %s <file>.hc <file>.prop:\nCompile a .hc file into a .prop file.\n",argv[0]);
		return 1;
	}
	UnlinkedPropellant prop;
	PropellantObject obj=compileFile(args[0]);
	prop.objects.push_back(obj);
	std::string abi="abi.json";
	LinkedPropellant lprop=link(prop,abi);
	std::vector<uint8_t> buf;
	if (false){
		StrippedPropellant sprop=strip(lprop);
		buf=serializeStripped(sprop);
	} else {
		buf=serialize(lprop);	
	}
	std::ofstream out(args[1], std::ios::binary);
	out.write(reinterpret_cast<const char*>(buf.data()), buf.size());
	return 0;
}