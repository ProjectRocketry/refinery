#include "../libpropfile/prop_hc_interface.h"
PropellantObject compileFile(std::string filename);
LinkedPropellant link(const UnlinkedPropellant& prop, std::string& abiFilepath);
StrippedPropellant strip(const LinkedPropellant& prop);