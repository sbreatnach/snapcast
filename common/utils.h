/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef UTILS_H
#define UTILS_H

#include "common/strCompat.h"
#include "common/utils/string_utils.h"

#include <functional>
#include <cctype>
#include <locale>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cerrno>
#include <iterator>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iomanip>
#ifndef FREEBSD
#include <sys/sysinfo.h>
#endif
#include <sys/utsname.h>
#ifdef MACOS
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOTypes.h>
#endif
#ifdef ANDROID
#include <sys/system_properties.h>
#endif
#include <asio.hpp>


using asio::ip::tcp;
namespace strutils = utils::string;



static std::string execGetOutput(const std::string& cmd)
{
	std::string fullCmd = cmd
#ifndef WINDOWS
		+ " 2> /dev/null"
#endif
	;
	std::shared_ptr<FILE> pipe(popen(fullCmd.c_str(), "r"), pclose);
	if (!pipe)
		return "";
	char buffer[1024];
	std::string result = "";
	while (!feof(pipe.get()))
	{
		if (fgets(buffer, 1024, pipe.get()) != NULL)
			result += buffer;
	}
	return strutils::trim(result);
}


#ifdef ANDROID
static std::string getProp(const std::string& key, const std::string& def = "")
{
    std::string result(def);
    char cresult[PROP_VALUE_MAX+1];
    if (__system_property_get(key.c_str(), cresult) > 0)
        result = cresult;
    return result;
}
#endif


static std::string getOS()
{
	std::string os;
#ifdef ANDROID
	os = strutils::trim_copy("Android " + getProp("ro.build.version.release"));
#elif WINDOWS
	if (IsWindows10OrGreater())
		os = "Windows 10";
	else if (IsWindows8Point1OrGreater())
		os = "Windows 8.1";
	else if (IsWindows8OrGreater())
		os = "Windows 8";
	else if (IsWindows7SP1OrGreater())
		os = "Windows 7 SP1";
	else if (IsWindows7OrGreater())
		os = "Windows 7";
	else if (IsWindowsVistaSP2OrGreater())
		os = "Windows Vista SP2";
	else if (IsWindowsVistaSP1OrGreater())
		os = "Windows Vista SP1";
	else if (IsWindowsVistaOrGreater())
		os = "Windows Vista";
	else if (IsWindowsXPSP3OrGreater())
		os = "Windows XP SP3";
	else if (IsWindowsXPSP2OrGreater())
		os = "Windows XP SP2";
	else if (IsWindowsXPSP1OrGreater())
		os = "Windows XP SP1";
	else if (IsWindowsXPOrGreater())
		os = "Windows XP";
	else
		os = "Unknown Windows";
#else
	os = execGetOutput("lsb_release -d");
	if ((os.find(":") != std::string::npos) && (os.find("lsb_release") == std::string::npos))
		os = strutils::trim_copy(os.substr(os.find(":") + 1));
	if (os.empty())
	{
		os = strutils::trim_copy(execGetOutput("grep /etc/os-release /etc/openwrt_release -e PRETTY_NAME -e DISTRIB_DESCRIPTION"));
		if (os.find("=") != std::string::npos)
		{
			os = strutils::trim_copy(os.substr(os.find("=") + 1));
			os.erase(std::remove(os.begin(), os.end(), '"'), os.end());
			os.erase(std::remove(os.begin(), os.end(), '\''), os.end());
		}
	}
#endif
+#ifndef WINDOWS
	if (os.empty())
	{
		utsname u;
		uname(&u);
		os = u.sysname;
	}
#endif
	return strutils::trim_copy(os);
}


static std::string getHostName()
{
#ifdef ANDROID
	std::string result = getProp("net.hostname");
	if (!result.empty())
		return result;
#endif
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	return hostname;
}


static std::string getArch()
{
	std::string arch;
#if defined(ANDROID)
	arch = getProp("ro.product.cpu.abi");
	if (!arch.empty())
		return arch;
#elif defined(WINDOWS)
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	switch (sysInfo.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		arch = "amd64";
		break;

	case PROCESSOR_ARCHITECTURE_ARM:
		arch = "arm";
		break;

	case PROCESSOR_ARCHITECTURE_IA64:
		arch = "ia64";
		break;

	case PROCESSOR_ARCHITECTURE_INTEL:
		arch = "intel";
		break;

	default:
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
		arch = "unknown";
		break;
	}
#else
	arch = execGetOutput("arch");
	if (arch.empty())
		arch = execGetOutput("uname -i");
	if (arch.empty() || (arch == "unknown"))
		arch = execGetOutput("uname -m");
#endif
	return strutils::trim_copy(arch);
}


static long uptime()
{
#if defined(WINDOWS)
	// NB: this rolls over after 49.7 days
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(GetTickCount())).count();
#elif defined(FREEBSD)
	std::string uptime = execGetOutput("sysctl kern.boottime");
	if ((uptime.find(" sec = ") != std::string::npos) && (uptime.find(",") != std::string::npos))
	{
		uptime = strutils::trim_copy(uptime.substr(uptime.find(" sec = ") + 7));
		uptime.resize(uptime.find(","));
		timeval now;
		gettimeofday(&now, NULL);
		try
		{
			return now.tv_sec - cpt::stoul(uptime);
		}
		catch (...)
		{
		}
	}
	return 0;
#else
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime;
#endif
}


/// http://stackoverflow.com/questions/2174768/generating-random-uuids-in-linux
static std::string generateUUID()
{
	static bool initialized(false);
	if (!initialized)
	{
		std::srand(std::time(0));
		initialized = true;
	}
	std::stringstream ss;
	ss << std::setfill('0') << std::hex  
		<< std::setw(4) << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff)
		<< "-" << std::setw(4) << (std::rand() % 0xffff)
		<< "-" << std::setw(4) << (std::rand() % 0xffff)
		<< "-" << std::setw(4) << (std::rand() % 0xffff)
		<< "-" << std::setw(4) << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff);
	return ss.str();
}


/// https://gist.github.com/OrangeTide/909204
static std::string getMacAddress(tcp::socket* socket_)
{
	char mac[19];
#ifdef WINDOWS
	std::string address = socket_->local_endpoint().address().to_string();
	IP_ADAPTER_INFO* first;
	IP_ADAPTER_INFO* pos;
	ULONG bufferLength = sizeof(IP_ADAPTER_INFO);
	first = (IP_ADAPTER_INFO*)malloc(bufferLength);

	if (GetAdaptersInfo(first, &bufferLength) == ERROR_BUFFER_OVERFLOW)
	{
		free(first);
		first = (IP_ADAPTER_INFO*)malloc(bufferLength);
	}

	if (GetAdaptersInfo(first, &bufferLength) == NO_ERROR)
		for (pos = first; pos != NULL; pos = pos->Next)
		{
			IP_ADDR_STRING* firstAddr = &pos->IpAddressList;
			IP_ADDR_STRING* posAddr;
			for (posAddr = firstAddr; posAddr != NULL; posAddr = posAddr->Next)
				if (_stricmp(posAddr->IpAddress.String, address.c_str()) == 0)
				{
					sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
									pos->Address[0], pos->Address[1], pos->Address[2],
									pos->Address[3], pos->Address[4], pos->Address[5]);

					free(first);
					return mac;
				}
		}
	else
		free(first);

#else
	int sock = socket_->native_handle();
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[16384];
	int success = 0;

	if (sock < 0)
		return "";

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) != 0)
		return "";

	struct ifreq* it = ifc.ifc_req;
	for (int i=0; i<ifc.ifc_len;) 
	{
		/// some systems have ifr_addr.sa_len and adjust the length that way, but not mine. weird */
#ifdef FREEBSD
		size_t len = IFNAMSIZ + it->ifr_addr.sa_len;
#else
		size_t len = sizeof(*it);
#endif

		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
		{
			if (!(ifr.ifr_flags & IFF_LOOPBACK)) // don't count loopback
			{
#ifdef MACOS
				/// Dirty Mac version
				struct ifaddrs *ifap, *ifaptr;
				unsigned char *ptr;

				if (getifaddrs(&ifap) == 0) 
				{
					for (ifaptr = ifap; ifaptr != NULL; ifaptr = ifaptr->ifa_next) 
					{
//						std::cout << ifaptr->ifa_name << ", " << ifreq->ifr_name << "\n";
						if (strcmp(ifaptr->ifa_name, it->ifr_name) != 0)
							continue;
						if (ifaptr->ifa_addr->sa_family == AF_LINK) 
						{
							ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)ifaptr->ifa_addr);
							char mac[19];
							sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
							if (strcmp(mac, "00:00:00:00:00:00") == 0)
								continue;
							freeifaddrs(ifap);
							return mac;
						}
					}
					freeifaddrs(ifap);
				}   
#endif

#ifdef FREEBSD
				if (ioctl(sock, SIOCGIFMAC, &ifr) == 0)
#else
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
#endif
				{
					success = 1;
					break;
				}
				else
				{
					std::stringstream ss;
					ss << "/sys/class/net/" << ifr.ifr_name << "/address";
					std::ifstream infile(ss.str().c_str());
					std::string line;
					if (infile.good() && std::getline(infile, line))
					{
						strutils::trim(line);
						if ((line.size() == 17) && (line[2] == ':'))
							return line;
					}
				}
			}
		}
		else { /* handle error */ }

		it = (struct ifreq*)((char*)it + len);
		i += len;
	}

	if (!success)
		return "";

#ifndef FREEBSD
	sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
		(unsigned char)ifr.ifr_hwaddr.sa_data[0], (unsigned char)ifr.ifr_hwaddr.sa_data[1], (unsigned char)ifr.ifr_hwaddr.sa_data[2],
		(unsigned char)ifr.ifr_hwaddr.sa_data[3], (unsigned char)ifr.ifr_hwaddr.sa_data[4], (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
#else
	sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
		(unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[0], (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[1], (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[2], 
		(unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[3], (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[4], (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[5]);
#endif
#endif
	return mac;
}


static std::string getHostId(const std::string defaultId = "")
{
	std::string result = strutils::trim_copy(defaultId);

	/// the Android API will return "02:00:00:00:00:00" for WifiInfo.getMacAddress(). 
	/// Maybe this could also happen with native code
	if (!result.empty() && (result != "02:00:00:00:00:00") && (result != "00:00:00:00:00:00"))
		return result;

#ifdef MACOS
	/// https://stackoverflow.com/questions/933460/unique-hardware-id-in-mac-os-x
	/// About this Mac, Hardware-UUID
	char buf[64];
	io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
	CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
	IOObjectRelease(ioRegistryRoot);
	if (CFStringGetCString(uuidCf, buf, 64, kCFStringEncodingMacRoman))
		result = buf;
	CFRelease(uuidCf);
#elif ANDROID
	result = getProp("ro.serialno");
#endif

//#else
//	// on embedded platforms it's
//  // - either not there
//  // - or not unique, or changes during boot
//  // - or changes during boot
//	std::ifstream infile("/var/lib/dbus/machine-id");
//	if (infile.good())
//		std::getline(infile, result);
//#endif
	strutils::trim(result);
	if (!result.empty())
		return result;

	/// The host name should be unique enough in a LAN
	return getHostName();
}


#endif


