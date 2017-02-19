#ifndef __PARAMS_H
#define __PARAMS_H

#include <vector>
#include <string>
#include <map>

#include "enums.h"

class params {
public:
   params(int argc, char * const * argv); // pointer to a const string (poitner to a const pointer to a char).
   std::string substitute( const std::string & source ) const;

   const std::string & getVersion() const { return mVersion; }
   eCommand getCommand() const { return mCmd; }
   std::string getCommandStr() const { return mCmdStr; }
   eLogLevel getLogLevel() const { return mLogLevel; }

   edServiceOutput supportCallMode() const { return mServiceOutput_supportcalls ? kORaw : kOSuppressed; }
   edServiceOutput serviceCmdMode() const { return mServiceOutput_servicecmd ? kORaw : kOSuppressed; }

   const std::vector<std::string> & getArgs() const { return mArgs; }
   int numArgs() const { return mArgs.size(); }
   const std::string & getArg(int n) const;
  
   const std::vector<std::string> & getOptions() const { return mOptions; }
   bool isDevelopmentMode() const { return mDevelopmentMode; }
   void setDevelopmentMode(bool dev) const { mDevelopmentMode = dev; }
   bool doPause() const { return mPause; }

   bool isdrunnerCommand(std::string c) const;
   bool isHook(std::string c) const;
   eCommand getdrunnerCommand(std::string c) const;

private:
   std::string mVersion;
   eCommand mCmd;
   std::string mCmdStr;
   std::vector<std::string> mArgs;
   eLogLevel mLogLevel;
   bool mPause;
   const std::map<std::string, eCommand> mCommandList;

   bool mServiceOutput_supportcalls;
   bool mServiceOutput_servicecmd;

   std::vector<std::string> mOptions;
   params();
   void _setdefaults();
   void _parse(int argc, char * const * argv);


   mutable bool mDevelopmentMode;
};

#endif
