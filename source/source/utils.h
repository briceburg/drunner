#ifndef __UTILS_H
#define __UTILS_H

#include <vector>
#include <string>
#include <sys/stat.h>

#include <Poco/Path.h>
#include <Poco/Process.h>

#include "enums.h"
#include "cresult.h"
#include "basen.h"

typedef std::vector<std::string> tVecStr;

class CommandLine
{
public:
   CommandLine() {}
   CommandLine(std::string c) : command(c) {}
   CommandLine(std::string c, const std::vector<std::string> & a) : command(c), args(a) { }
   void logcommand(std::string prefix, eLogLevel ll=kLDEBUG) const;
   void setfromvector(const std::vector<std::string> & v);

   std::string command;
   std::vector<std::string> args;
};

// A very simple hasing of strings, useful for switch statements. Case sensitive.
constexpr unsigned int s2i(const char* str, int h = 0)
{
   return !str[h] ? 5381 : (s2i(str, h + 1) * 33) ^ str[h];
}

// Shell utilities
namespace utils
{

#ifdef _WIN32
   const std::string kCODE_S = "";
   const std::string kCODE_E = "";
#else
   const std::string kCODE_S="\e[32m";
   const std::string kCODE_E="\e[0m";
#endif

   //cResult _makedirectories(Poco::Path path);
   //cResult makedirectories(Poco::Path path,mode_t mode);
   cResult makedirectory(Poco::Path d, mode_t mode);
//   void makesymlink(Poco::Path file, Poco::Path link);

   cResult deltree(Poco::Path s);
   cResult delfile(Poco::Path fullpath);

   void movetree(const std::string & src, const std::string & dst);
   bool getFolders(const std::string & parent, std::vector<std::string> & folders);

   bool fileexists(const Poco::Path& name);
   bool fileexists(const Poco::Path& parent, std::string name);
   bool commandexists(std::string command);
   
   int runcommand(const CommandLine & operation, std::string &out);
   int runcommand_stream(const CommandLine & operation, edServiceOutput outputMode, Poco::Path initialDirectory, const Poco::Process::Env & env, std::string * out);

   bool findStringIC(const std::string & strHaystack, const std::string & strNeedle);
   std::string replacestring(std::string subject, const std::string& search, const std::string& replace);
   std::string alphanumericfilter(std::string s, bool whitespace);
   bool wordmatch(std::string s, std::string word);

   std::string getTime();
   std::string getPWD();

   std::string getenv(std::string envParam);

   void getAllServices(std::vector<std::string> & services);

   cResult split_in_args(std::string command, CommandLine & cl);

   void logfile(std::string fname);

   std::string base64encode(std::string s);
   std::string base64decode(std::string s);

   std::string base64encodeWithEquals(std::string s); // able to be decoded with base64 -d in Linux.

   void str2vecstr(std::string s, std::vector<std::string> & vecstr);
   std::string vecstr2str(std::vector<std::string> vecstr);

   class tempfolder
   {
   public:
      tempfolder(Poco::Path d);
      ~tempfolder();
      Poco::Path getpath() const;
      
   private:
      void die(std::string msg);
      void tidy();
      Poco::Path mPath;
   };


   std::string _pad(std::string x, unsigned int w);
   inline int _max(int a, int b) { return (a > b) ? a : b; }

} // namespace


#endif
