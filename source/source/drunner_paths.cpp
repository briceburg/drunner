#include <Poco/UnicodeConverter.h>
#include <Poco/File.h>

#include "drunner_paths.h"
#include "utils.h"
#include "globallogger.h"

#ifdef _WIN32
#include <ShlObj.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <unistd.h>
#include <mach-o/dyld.h>
#else
#include <linux/limits.h>
#include <unistd.h>
#endif

Poco::Path drunnerPaths::getPath_Root() {
   Poco::Path drunnerdir = Poco::Path::home();
   drunnerdir.makeDirectory();
   poco_assert(drunnerdir.isDirectory());
   drunnerdir.pushDirectory(".drunner");
   return drunnerdir;
}

Poco::Path drunnerPaths::getPath_Exe()
{
   Poco::Path p;
#ifdef _WIN32
   std::vector<char> pathBuf;
   DWORD copied = 0;
   do {
      pathBuf.resize(pathBuf.size() + MAX_PATH);
      copied = GetModuleFileNameA(0, &pathBuf.at(0), pathBuf.size());
   } while (copied >= pathBuf.size());
   pathBuf.resize(copied);

   std::string path(pathBuf.begin(), pathBuf.end());
   p=Poco::Path(path);
#elif __APPLE__
   char pathBuf[PATH_MAX]; // Note: may not be long enough for pathological cases!
   uint32_t bufsize = sizeof(pathBuf);
   if (_NSGetExecutablePath(pathBuf, &bufsize) != 0)
      logmsg(kLERROR, "Couldn't get path to drunner executable!");
   p=Poco::Path(pathBuf);
#else
   char buff[PATH_MAX];
   ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
   if (len==-1)
      logmsg(kLERROR, "Couldn't get path to drunner executable!");

   buff[len] = '\0';
   p=Poco::Path(buff);
#endif
   poco_assert(p.isFile());
   poco_assert(utils::fileexists(p));

   return p;
}

Poco::Path drunnerPaths::getPath_Exe_Target()
{
#ifdef _WIN32
   Poco::Path exelocation = drunnerPaths::getPath_Bin();
   exelocation.setFileName("drunner.exe");
   return exelocation;
#else
   return getPath_Exe();
#endif
}

Poco::Path drunnerPaths::getPath_Bin()
{
   return getPath_Root().pushDirectory("bin");
} // bin subfolder

Poco::Path drunnerPaths::getPath_dServices()
{
   return getPath_Root().pushDirectory("dServices");
}

Poco::Path drunnerPaths::getPath_Temp()
{
   return getPath_Root().pushDirectory("temp");
}

Poco::Path drunnerPaths::getPath_HostVolumes()
{
   return getPath_Root().pushDirectory("hostVolumes");
}

Poco::Path drunnerPaths::getPath_Settings()
{
   return getPath_Root().pushDirectory("settings");
}

Poco::Path drunnerPaths::getPath_Logs()
{
   return getPath_Root().pushDirectory("logs");
}


Poco::Path drunnerPaths::getPath_drunnerSettings_json()
{
   return getPath_Settings().setFileName("drunnerSettings.json");
}

std::string drunnerPaths::getdrunnerUtilsImage()
{
   return  "drunner/drunner_utils";
}

