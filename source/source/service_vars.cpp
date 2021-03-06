#include <fstream>

#include "service_vars.h"
#include "service_paths.h"

//serviceVars::serviceVars(std::string servicename, const std::vector<envDef> & config) :
//   persistvariables(servicename, servicePaths(servicename).getPathServiceVars(), config)
//{
//   _extendconfig();
//}
#include "dassert.h"
#include "lua.hpp"
#include "service_lua.h"


// needs to be called from ctor.
std::vector<envDef> loadServiceVariablesLua(std::string servicename);

serviceVars::serviceVars(std::string servicename) :
   persistvariables(servicename, servicePaths(servicename).getPathServiceVars(), loadServiceVariablesLua(servicename))
{
   _extendconfig();

   cResult r = loadvariables();
   if (!r.success())
      logmsg(kLDEBUG, "Failed to load service variables.");
}

std::string serviceVars::getImageName() const
{
   std::string iname = getVal("IMAGENAME");
   if (iname.length() == 0) logmsg(kLWARN, "IMAGENAME not defined.");
   return iname;
}

std::string serviceVars::getServiceName() const
{
   std::string sname = getVal("SERVICENAME");
   if (sname.length() == 0) logmsg(kLWARN, "SERVICENAME not defined.");
   return sname;
}

void serviceVars::setImageName(std::string imagename)
{
   setVal("IMAGENAME", imagename);
}

std::string serviceVars::getTempBackupFolder() const
{
   return getVal("TEMPBACKUPFOLDER");
}

void serviceVars::setTempBackupFolder(std::string folder)
{
   setVal("TEMPBACKUPFOLDER", folder);
}


void serviceVars::_extendconfig()
{
   _addConfig(envDef("SERVICENAME", mName, "Name of Service", ENV_DEFAULTS_MEM)); // in memory, no need to persist this.
   _addConfig(envDef("TEMPBACKUPFOLDER", mName, "Temporary location for backup files", ENV_DEFAULTS_MEM)); // in memory, no need to persist this.

   _addConfig(envDef("IMAGENAME", "", "Image name", ENV_PERSISTS));
}


// ------------------------------------------------------------------------------------------------------

// needs to be called from ctor.
std::vector<envDef> loadServiceVariablesLua(std::string servicename)
{
   std::vector<envDef> defs;

   std::string path = servicePaths(servicename).getPathServiceLua().toString();
   std::ifstream infile(path);
   std::string line;
   std::string search = "addconfig(";

   if (!infile.is_open())
   {
      logmsg(kLDEBUG, "Couldn't open service.lua at "+path);
      logmsg(kLWARN, servicename + " is not installed (no service.lua).");
      return defs;
   }

   while (std::getline(infile, line))
   { // search for addconfig(
      size_t pos = line.find(search);
      size_t dqpos = line.find("--"); // skip if it's a comment line.
      if (pos != std::string::npos && (dqpos==std::string::npos || dqpos>pos))
      {
         size_t pos2 = line.find(")",pos);
         pos += search.length(); // move past the search string.

         if (pos2 != std::string::npos && pos2 > pos)
         {
            std::string argstr = line.substr(pos, pos2 - pos);

            std::vector<std::string> args;
            pos = 0;
            while ((pos= argstr.find("\"", pos)) != std::string::npos)
            {
               ++pos;
               if ((pos2 = argstr.find("\"", pos)) != std::string::npos)
                  args.push_back(argstr.substr(pos, pos2 - pos));
               pos = pos2 + 1;
            }

            if (args.size() == 3)
            {
               defs.push_back(envDef(args[0], args[1], args[2], ENV_PERSISTS | ENV_USERSETTABLE));
               logmsg(kLDEBUG, "addconfig: name=" + args[0] + ", default=" + args[1] + ", desc=" + args[2]);
            }
            else
               logmsg(kLERROR, "addconfig requires three quoted arguments on this line:\n" + line);
         }
      }
   }

   return defs;
}