#include <sys/stat.h>
#include <fstream>

#include <Poco/String.h>
#include <Poco/File.h>

#include "command_dev.h"
#include "utils.h"
#include "utils_docker.h"
#include "globallogger.h"
#include "globalcontext.h"
#include "exceptions.h"
#include "settingsbash.h"
#include "drunner_setup.h"
#include "service.h"
#include "servicehook.h"
#include "sh_servicevars.h"
#include "chmod.h"
#include "validateimage.h"
#include "service_install.h"
#include "utils_docker.h"
#include "service.h"
#include "drunner_paths.h"
#include "generate.h"
#include "dassert.h"

service_install::service_install(std::string servicename) : servicePaths(servicename)
{ // no imagename specified. Load from service_lua.
   servicelua::luafile lf(servicename);
   if (lf.loadlua()!=kRSuccess)
      logmsg(kLWARN,"No imagename specified and unable to read service.lua.");
   else
      mImageName = lf.getImageName();
}

service_install::service_install(std::string servicename, std::string imagename) : servicePaths(servicename), mImageName(imagename)
{
}

void service_install::_createVolumes(std::vector<std::string> & volumes)
{
   if (volumes.size() == 0)
      logmsg(kLDEBUG, "[No volumes declared to be managed by drunner]");
   else
      for (const auto & v : volumes)
      {
         logmsg(kLDEBUG, "Checking status of volume " + v);

         // each service may be running under a different userid.
         if (utils_docker::dockerVolExists(v))
            logmsg(kLINFO, "A docker volume already exists for " + v + ", reusing it.");
         else
            utils_docker::createDockerVolume(v);

         // set permissions on volume.
         CommandLine cl("docker", { "run", "--rm", "-v",
            v + ":" + "/tempmount", drunnerPaths::getdrunnerUtilsImage(),
            "chmod","0777","/tempmount" } );
      
         int rval = utils::runcommand_stream(cl, GlobalContext::getParams()->supportCallMode());
         if (rval != 0)
            fatal("Failed to set permissions on docker volume " + v);

         logmsg(kLDEBUG, "Set permissions to allow access to volume " + v);
      }
   logmsg(kLDEBUG, "Finished checking volumes.");
}

void service_install::_ensureDirectoriesExist() const
{
   // create service's drunner and temp directories on host.
   utils::makedirectory(getPathdService(), S_700);
   utils::makedirectory(getPathServiceVars().parent(), S_700);
}

cResult service_install::_recreate(bool updating)
{
   drunner_assert(mImageName.length() > 0, "Can't recreate service "+mName + " - image name could not be determined.");

   if (updating && utils::fileexists(getPathServiceLua()))
   { // pull all containers used by the dService.
      servicelua::luafile syf(mName);
      if (syf.loadlua()==kRSuccess)
         for (auto c : syf.getContainers())
            utils_docker::pullImage(c);
   }
   
   try
   {
      // nuke any existing dService files on host (but preserve volume containers!).
      if (utils::fileexists(getPathdService()))
      {
         Poco::File spath(getPathdService());
         spath.remove(true); // recursively delete.
      }

      // notice for hostVolumes.
      if (utils::fileexists(getPathHostVolume()))
         logmsg(kLINFO, "A drunner hostVolume already exists for " + getName() + ", reusing it.");

      // create the basic directories.
      _ensureDirectoriesExist();

      // copy files to service directory on host.
      CommandLine cl("docker", { "run","--rm","-u","root","-v",
         getPathdService().toString() + ":/tempcopy", mImageName, "/bin/bash", "-c" });
#ifdef _WIN32
      cl.args.push_back("cp /drunner/* /tempcopy/ && chmod a+rw /tempcopy/*");
#else
      uid_t uid = getuid();
      cl.args.push_back("cp /drunner/* /tempcopy/ && chmod u+rw /tempcopy/* && chown -R "+std::to_string(uid)+" /tempcopy/*");
#endif
      std::string op;
      if (0 != utils::runcommand(cl, op, utils::kRC_Defaults))
         logmsg(kLERROR, "Could not copy the service files. You will need to reinstall the service.\nError:\n" + op);

      // write out service configuration for the dService.
      servicelua::luafile syf(mName);
      if (syf.loadlua()!=kRSuccess)
         fatal("Corrupt dservice - couldn't read service.lua.");
      if (syf.saveVariables() != kRSuccess)
         fatal("Could not write out service variables.");

      // make sure we have the latest of all containers.
      bool foundmain = false;
      for (const auto & entry : syf.getContainers())
      {
         utils_docker::pullImage(entry);
         if (utils::stringisame(entry, mImageName))
            foundmain = true;
      }
      if (!foundmain)
         logmsg(kLWARN, "The main dService container " + mImageName + " was not present in the containers list in the service.lua file.");
      
      // create volumes, with variables substituted.
      std::vector<std::string> vols;
      syf.getManageDockerVolumeNames(vols);
      _createVolumes(vols);

      // create launch script
      _createLaunchScript();
   }

   catch (const eExit & e) {
      // We failed. tidy up.
      if (utils::fileexists(getPathdService()))
         utils::deltree(getPathdService());

      throw (e);
   }

   return kRSuccess;
}

cResult service_install::install()
{
   drunner_assert(mImageName.length() > 0, "Can't install service " + mName + " - image name could not be determined.");

   logmsg(kLDEBUG, "Installing " + mName + " at " + getPathdService().toString() + ", using image " + mImageName);
	if (utils::fileexists(getPathdService()))
		logmsg(kLERROR, "Service already exists. Try:\n drunner update " + getName());

	// make sure we have the latest version of the service.
   utils_docker::pullImage(mImageName);

	logmsg(kLDEBUG, "Attempting to validate " + mImageName);
   validateImage::validate(mImageName);

   _recreate(false);

   servicehook hook(mName, "install");
   hook.endhook();

   logmsg(kLINFO, "Installation complete - try running " + mName+ " now!");
   return kRSuccess;
}

cResult service_install::uninstall()
{
   cResult rval = kRNoChange;

   if (!utils::fileexists(getPathdService()))
      logmsg(kLERROR, "Can't uninstall " + mName + " - it does not exist.");

   try
   {
      servicehook hook(mName, "uninstall");
      hook.starthook();
   }
   catch (const eExit &)
   {
      logmsg(kLWARN, "Installation damaged, unable to use uninstall hook.");
   }

   // delete the service tree.
   logmsg(kLINFO, "Deleting all of the dService files");
   rval += utils::deltree(getPathdService());

   if (utils::fileexists(getPathdService()))
      logmsg(kLERROR, "Uninstall failed - couldn't delete " + getPathdService().toString());

   // delete the launch script
   rval += _removeLaunchScript();

   logmsg(kLINFO, "Uninstalled " + getName());
   return rval;
}

cResult service_install::obliterate()
{
   cResult rval = kRNoChange;

   if (utils::fileexists(getPathServiceLua()))
   {
      try
      {
         servicehook hook(mName, "obliterate");
         hook.starthook();

         logmsg(kLDEBUG, "Obliterating all the docker volumes - data will be gone forever.");
         // [start] deleting docker volumes.
         std::vector<std::string> vols;

         servicelua::luafile lf(mName);
         if (lf.loadlua() == kRSuccess)
         {
            lf.getManageDockerVolumeNames(vols);
            for (const auto & entry : vols)
            {
               logmsg(kLINFO, "Obliterating docker volume " + entry);
               std::string op;
               CommandLine cl("docker", { "volume", "rm",entry });
               if (0 != utils::runcommand(cl, op, utils::kRC_Defaults))
               {
                  logmsg(kLWARN, "Failed to remove " + entry + ":");
                  logmsg(kLWARN, op);
                  rval += kRError;
               }
               else
                  rval += kRSuccess;
            }
         }
      }
      catch (const eExit &)
      {
         logmsg(kLWARN, "Installation damaged, unable to delete docker volumes.");
      }
   }

   if (utils::fileexists(getPathdService()))
   { // delete the service tree.
      logmsg(kLINFO, "Obliterating all of the dService files.");
      cResult result = utils::deltree(getPathdService());
      rval += result;
      if (result != kRSuccess)
         logmsg(kLINFO, "Failed to delete the dService files.");
   }

   // delete the host volumes
   if (utils::fileexists(getPathHostVolume()))
   {
      logmsg(kLINFO, "Obliterating the hostVolume (includes configuration).");
      cResult result = utils::deltree(getPathHostVolume());
      rval += result;
      if (result != kRSuccess)
         logmsg(kLINFO, "Failed to delete the hostVolume files.");
   }

   // delete the launch script
   rval += _removeLaunchScript();

   if (rval == kRNoChange)
      logmsg(kLWARN, "Couldn't find any trace of dService " + getName() + " - no changes made.");
   else if (rval == kRError)
      logmsg(kLWARN, "Obliterated what we could, but system is not clean.");
   else
      logmsg(kLINFO, "Obliterated " + getName());
   return rval;
}



cResult service_install::recover()
{
   if (utils::fileexists(getPathdService()))
      uninstall();

   return install();
}

cResult service_install::update()
{ // update the service (recreate it)
   drunner_assert(mImageName.length() > 0, "Can't update service " + mName + " - image name could not be determined.");

   servicehook hook(mName, "update");
   hook.starthook();

   _recreate(true);

   hook.endhook();

   return kRSuccess;
}



void service_install::_createLaunchScript() const
{
#ifdef _WIN32
   Poco::Path target = drunnerPaths::getPath_Bin().setFileName(getName()+".bat");
   std::string vdata = R"EOF(@echo off
drunner servicecmd __SERVICENAME__ %*
)EOF";
#else
   Poco::Path target = drunnerPaths::getPath_Bin().setFileName(getName());
   std::string vdata = R"EOF(#!/bin/bash
drunner servicecmd "__SERVICENAME__" "$@"
)EOF";
#endif

   vdata = utils::replacestring(vdata, "__SERVICENAME__", mName);
   generate(target, S_755, vdata);
}

cResult service_install::_removeLaunchScript() const
{
   cResult rval = kRNoChange;

#ifdef _WIN32
   Poco::Path target = drunnerPaths::getPath_Bin().setFileName(getName() + ".bat");
#else
   Poco::Path target = drunnerPaths::getPath_Bin().setFileName(getName());
#endif

   if (!utils::fileexists(target))
   {
      logmsg(kLDEBUG, "Launch script " + target.toString() + " does not exist.");
      return rval;
   }

   rval += utils::delfile(target);
   
   if (rval==kRSuccess)
      logmsg(kLDEBUG, "Deleted " + target.toString());
   else
      logmsg(kLWARN, "Couldn't remove launch script");
   return rval;
}