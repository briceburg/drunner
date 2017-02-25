#include <fstream>
#include <cereal/archives/json.hpp>
#include "Poco/String.h"

#include "proxy.h"
#include "utils.h"
#include "drunner_paths.h"
#include "dassert.h"
#include "utils_docker.h"

proxy::proxy()
{
}

cResult proxy::proxyenable(proxydatum pd)
{
   logmsg(kLINFO, "Enabling HTTPS reverse proxy for " + pd.servicename);

   cResult r = pd.valid();
   if (r.error())
      return r;

   // load existing settings.
   r = load();
   if (r.error())
      return r;

   for (unsigned int i = 0; i < mData.mProxyData.size(); ++i)
      if (Poco::icompare(mData.mProxyData[i].servicename, pd.servicename) == 0)
      {
         if (mData.mProxyData[i] == pd)
            return kRNoChange; // already enabled.

         mData.mProxyData.erase(mData.mProxyData.begin() + i);
      }

   mData.mProxyData.push_back(pd);

   r = proxyconfigchanged();
   return r;
}

cResult proxy::proxydisable(std::string service)
{
   // load existing settings.
   cResult r = load();
   if (r.error())
      return r;

   int todelete = -1;
   for (unsigned int i = 0; i < mData.mProxyData.size(); ++i)
      if (Poco::icompare(mData.mProxyData[i].servicename, service) == 0)
         todelete = i;

   if (todelete == -1)
      return kRNoChange; // already disabled.

   mData.mProxyData.erase(mData.mProxyData.begin() + todelete);

   r = proxyconfigchanged();
   return r;
}

cResult proxy::proxyregen()
{
   cResult r = load();
   if (r.success())
      r += generate();
   if (r.success())
      r += restart();
   return r;
}

cResult proxy::proxyconfigchanged()
{
   // save updated settings
   cResult r = save();
   if (r.error())
      return r;

   // generate the Caddyfile
   r += generate();
   if (r.error())
      return r;

   // restart the Caddy service
   r += restart();
   return r;
}

// generate the caddyfile in the shared volume
cResult proxy::generate()
{
   std::ostringstream oss;

   for (auto x : mData.mProxyData)
   {       
      std::string ip = utils_docker::getIPAddress(x.container, networkName());
      if (ip.length() == 0)
         fatal("The container " + x.container + " does not appear to be attached to the proxy network '" + networkName() + "'.");

      if (ip.length() == 0)
         logmsg(kLWARN, "Couldn't determine IP address for " + x.container + " - skipping proxy configuration.");
      else
      {
         bool fakemode = (Poco::icompare(x.mode, "fake") == 0);

         oss << "https://" << x.domain << " {" << std::endl;
         oss << "   proxy / " << ip << ":" << x.port << " {" << std::endl;
         oss << "      transparent" << std::endl;
         oss << "      websocket" << std::endl;
         oss << "   }" << std::endl;
         oss << "   gzip" << std::endl;
         oss << "   tls " <<
            (fakemode ? "self_signed" : x.email)
            << std::endl;

         if (!x.timeouts)
            oss << "   timeouts none" << std::endl;

         oss << "}" << std::endl << std::endl;

         oss << "http://" << x.domain << " {" << std::endl;
         oss << "   redir https://" << x.domain << std::endl;
         oss << "}" << std::endl << std::endl;
      }
   }

   std::string encoded_data = utils::base64encodeWithEquals(oss.str());
   logmsg(kLDEBUG, oss.str());

   // generate the caddy file in the drunner-proxy-dataVolume volume.
   CommandLine cl("docker", { "run","--rm",
      "-v",dataVolume()+":/data",
      drunnerPaths::getdrunnerUtilsImage(),
      "/bin/bash","-c",
      "echo " + encoded_data + " | base64 -d > /data/caddyfile" });

   std::string op;
   int rval = utils::runcommand(cl, op);
   if (rval != 0)
   {
      Poco::trimInPlace(op);
      return cError("Command failed: " + op);
   }

   return kRSuccess;
}

// restart the service
cResult proxy::restart()
{
   std::string op;
   if (utils_docker::dockerContainerRunning(containerName()))
   { // just send signal to restart it.
     //      docker exec <container> kill - SIGUSR1 1
      logmsg(kLINFO, "Reloading dRunner proxy settings (SIGUSR1)");
      CommandLine cl("docker", { "exec",containerName(),"kill","-SIGUSR1","1" });
      int rval = utils::runcommand(cl, op);
      if (rval != 0)
         return cError("Command failed: " + op);
      return kRSuccess;
   }

   if (utils_docker::dockerContainerExists(containerName()))
   { // get rid of old crufty container.
      logmsg(kLWARN, "Removing unexpected stopped proxy container! " + containerName());
      utils_docker::removeContainer(containerName());
   }


   if (mData.mProxyData.size() == 0)
      return kRSuccess; // no need for proxy!

   logmsg(kLDEBUG, "Starting dRunner proxy.");

   // Container is not running. Volume get auto-created if not already present.
   // Note that the /root/.caddy volume mapping is critical to not burn LetsEncrypt certs on
   // container restart and hit their issuing limit!
   CommandLine cl("docker",
   { 
      "run",
      "-v",dataVolume() + ":/data",
      "-v",rootVolume() + ":/root/.caddy",
      "--name",containerName(),
      "-p","80:80",
      "-p","443:443",
      "--restart=always",
      "--network="+networkName(),
      "-d",
      drunnerPaths::getdrunnerProxyImage() 
   });
   
   int rval = utils::runcommand(cl, op);
   if (rval != 0)
      return cError("Command failed: " + op);
   return kRSuccess;
}

cResult proxy::load()
{
   if (!utils::fileexists(saveFilePath()))
      return kRNoChange;

   // read the settings.
   std::ifstream is(saveFilePath().toString());
   if (is.bad())
      return cError("Unable to open " + saveFilePath().toString() + " for reading.");
   try
   {
      mData.mProxyData.clear();
      cereal::JSONInputArchive archive(is);
      archive(mData);
   }
   catch (const cereal::Exception & e)
   {
      return cError("Cereal exception on reading settings: " + std::string(e.what()));
   }

   return kRSuccess;
}

cResult proxy::save()
{
   logdbg("Creating " + saveFilePath().toString());
   std::ofstream os(saveFilePath().toString());
   if (os.bad() || !os.is_open())
      return cError("Unable to open " + saveFilePath().toString() + " for writing.");

   try
   {
      cereal::JSONOutputArchive archive(os);
      archive(mData);
   }
   catch (const cereal::Exception & e)
   {
      return cError("Cereal exception on writing settings: " + std::string(e.what()));
   }
   drunner_assert(utils::fileexists(saveFilePath()), "Failed to create settings at " + saveFilePath().toString());
   return kRSuccess;
}

Poco::Path proxy::saveFilePath()
{
   return drunnerPaths::getPath_Settings().setFileName("dRunner_Proxy.json");
}

cResult proxydatum::valid()
{
   if (servicename.length() == 0)
      return cError("Service name not set.");
   if (domain.length() == 0)
      return cError("Domain name not set.");
   if (container.length() == 0)
      return cError("Container not set.");
   if (port.length() == 0 || atoi(port.c_str())<=0)
      return cError("Port not set.");
   switch (s2i(mode.c_str()))
   {
   case s2i("fake"):
      return kRSuccess;

   case s2i("staging"):
      return cError("Staging not currently supported.");

   case s2i("production"):
      if (email.length() == 0)
         return cError("Email not set, required for staging and production modes.");
      if (email.find("@") == std::string::npos)
         return cError("Email field not set to an email address.");
      break;

   default:
      return cError("Unkown mode - must be fake, staging or production");
   }

   return kRSuccess;
}

bool proxydatum::operator ==(const proxydatum &b) const
{
   if (Poco::icompare(servicename, b.servicename) != 0) return false;
   if (Poco::icompare(domain, b.domain) != 0) return false;
   if (Poco::icompare(container, b.container) != 0) return false;
   if (Poco::icompare(port, b.port) != 0) return false;
   if (Poco::icompare(email, b.email) != 0) return false;
   if (Poco::icompare(mode, b.mode) != 0) return false;

   return true;
}