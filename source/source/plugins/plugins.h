#ifndef __PLUGINS_H
#define __PLUGINS_H

#include <deque>
#include <vector>
#include <string>
#include <memory>

#include <Poco/Path.h>

#include "cresult.h"
#include "utils.h"
#include "variables.h"
#include "service_lua.h"
#include "service_vars.h"

// ----------------------------------------------------------------------------------------

class plugin
{
public:
   virtual std::string getName() const = 0;
   virtual cResult runCommand() const = 0;
   virtual cResult runHook(std::string hook, std::vector<std::string> hookparams, const servicelua::luafile * lf, const serviceVars * sv) const = 0;

   virtual servicelua::CronEntry getCron() const = 0;
   virtual cResult runCron() const = 0;
};

// ----------------------------------------------------------------------------------------

// a plugin that has stored configuration (can be per service, or for the entire plugin)
class configuredplugin : public plugin
{
public:
   configuredplugin();
   virtual ~configuredplugin();

   virtual cResult runCommand(const CommandLine & cl, persistvariables & v) const = 0; // runCommand may wish to update system variables and save them, so we pass a non-const reference.
   virtual cResult showHelp() const = 0;
   virtual Poco::Path configurationFilePath() const = 0;

public:
   virtual cResult runCommand() const;
   cResult addConfig(std::string name, std::string description, std::string defaultval, configtype type, bool required, bool usersettable);

protected:
   const persistvariables getPersistVariables() const;
   cResult setAndSaveVariable(std::string key, std::string val) const;

   std::vector<Configuration> mConfiguration;
};

// ----------------------------------------------------------------------------------------

class plugins
{
public:
   plugins();

   void generate_plugin_scripts() const;
   cResult runcommand() const;
   cResult runhook(std::string hook, std::vector<std::string> hookparams, const servicelua::luafile * lf=NULL, const serviceVars * sv=NULL) const;

   void getPluginNames(std::vector<std::string> & names) const;
   servicelua::CronEntry getCronJob(std::string pluginName) const;
   cResult runCron(std::string pluginName) const;

private:
   std::deque< std::unique_ptr<plugin> > mPlugins;
};

#endif
