#ifndef __SERVICE_YML_H
#define __SERVICE_YML_H

#include <string>
#include <vector>

#include <Poco/Path.h>
#include "cresult.h"
#include "utils.h"
#include "service_paths.h"
#include "variables.h"
#include "lua.hpp"
#include "service_vars.h"

namespace servicelua
{
   struct Volume 
   {
      bool backup;
      bool external;
      std::string name;
   };

   struct Container
   {
      std::string name;
      bool runasroot;
   };

   struct BackupVol
   {
      std::string volumeName;
      std::string backupName;
   };

   class CronEntry
   {
   public:
      CronEntry() : offsetmin("0"), repeatmin("0"), function("") {}
      bool isvalid() const { return repeatmin != "0"; }

      std::string offsetmin;
      std::string repeatmin;
      std::string function;
   };

   // lua file.
   class luafile {
   public:
      luafile(std::string serviceName);
      ~luafile();
      
      // loads the lua file, initialises the variables, loads the variables if able.
      cResult loadlua();

      const std::vector<Container> & getContainers() const;
      const std::vector<Configuration> & getLuaConfigurationDefinitions() const;
      void getManageDockerVolumeNames(std::vector<std::string> & vols) const;
      void getBackupDockerVolumeNames(std::vector<BackupVol> & vols) const;
      const std::vector<CronEntry> & getCronEntries() const;

      // for service::serviceRunnerCommand
      cResult runCommand(const CommandLine & serviceCmd, serviceVars * sVars);

      bool isLoaded() { return mLuaLoaded; }

      std::string getServiceName() { return mServicePaths.getName(); }

      // for lua
      void addContainer(Container c);
      void addConfiguration(Configuration cf);
      void addVolume(Volume v);
      void addCronEntry(CronEntry c);
      Poco::Path getdRunDir() const;
      void setdRunDir(std::string p);

      Poco::Path getPathdService();
      serviceVars * getServiceVars();

   private:
      cResult _showHelp(serviceVars * sVars);

      const servicePaths mServicePaths;

      std::vector<Container> mContainers;
      std::vector<Configuration> mLuaConfigurationDefinitions; // This is not the full configuration for the service, just the parts defined by service.lua (e.g. missing IMAGENAME, DEVMODE).
      std::vector<Volume> mVolumes;
      std::vector<CronEntry> mCronEntries;

      lua_State * L;
      serviceVars * mSVptr;

      bool mLuaLoaded;
      bool mVarsLoaded;
      bool mLoadAttempt;

      Poco::Path mdRunDir;
   };

   void _register_lua_cfuncs(lua_State *L);

} // namespace



#endif
