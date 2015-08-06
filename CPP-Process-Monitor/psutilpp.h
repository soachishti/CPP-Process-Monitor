#ifndef PSUTILPP_H
#define PSUTILPP_H

#include <iostream>

#include <map>
#include <vector>
#include <tuple>

#include <string>
#include <tchar.h>

#include <algorithm>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
using namespace std;

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <stdio.h>
BOOL SetPrivilege(
	HANDLE hToken,          // token handle
	LPCTSTR Privilege,      // Privilege to enable/disable
	BOOL bEnablePrivilege   // TRUE to enable.  FALSE to disable
	);
#define RTN_OK 0
#define RTN_USAGE 1
#define RTN_ERROR 13


#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

using namespace std;

namespace psutilpp {
	typedef vector<map<string, string>> NetworkInfo;
	namespace Functions {
		double filetime2int(FILETIME ft)
		{
			ULARGE_INTEGER temp;
			temp.HighPart = ft.dwHighDateTime;
			temp.LowPart = ft.dwLowDateTime;
			return double(temp.QuadPart);
		}

		BOOL ListProcessModules(DWORD dwPID)
		{
			HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
			MODULEENTRY32 me32;

			// Take a snapshot of all modules in the specified process.
			hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID);
			if (hModuleSnap == INVALID_HANDLE_VALUE)
			{
				//Functions::printError(TEXT("CreateToolhelp32Snapshot (of modules)"));
				return(FALSE);
			}

			// Set the size of the structure before using it.
			me32.dwSize = sizeof(MODULEENTRY32);

			// Retrieve information about the first module,
			// and exit if unsuccessful
			if (!Module32First(hModuleSnap, &me32))
			{
				//Functions::printError(TEXT("Module32First"));  // show cause of failure
				CloseHandle(hModuleSnap);           // clean the snapshot object
				return(FALSE);
			}

			// Now walk the module list of the process,
			// and display information about each module
			do
			{
				_tprintf(TEXT("\n\n     MODULE NAME:     %s"), me32.szModule);
				_tprintf(TEXT("\n     Executable     = %s"), me32.szExePath);
				_tprintf(TEXT("\n     Process ID     = 0x%08X"), me32.th32ProcessID);
				_tprintf(TEXT("\n     Ref count (g)  = 0x%04X"), me32.GlblcntUsage);
				_tprintf(TEXT("\n     Ref count (p)  = 0x%04X"), me32.ProccntUsage);
				_tprintf(TEXT("\n     Base address   = 0x%08X"), (DWORD)me32.modBaseAddr);
				_tprintf(TEXT("\n     Base size      = %d"), me32.modBaseSize);

			} while (Module32Next(hModuleSnap, &me32));

			CloseHandle(hModuleSnap);
			return(TRUE);
		}

		BOOL ListProcessThreads(DWORD dwOwnerPID)
		{
			HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
			THREADENTRY32 te32;

			// Take a snapshot of all running threads  
			hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (hThreadSnap == INVALID_HANDLE_VALUE)
				return(FALSE);

			// Fill in the size of the structure before using it. 
			te32.dwSize = sizeof(THREADENTRY32);

			// Retrieve information about the first thread,
			// and exit if unsuccessful
			if (!Thread32First(hThreadSnap, &te32))
			{
				//Functions::printError(TEXT("Thread32First")); // show cause of failure
				CloseHandle(hThreadSnap);          // clean the snapshot object
				return(FALSE);
			}

			// Now walk the thread list of the system,
			// and display information about each thread
			// associated with the specified process
			do
			{
				if (te32.th32OwnerProcessID == dwOwnerPID)
				{
					_tprintf(TEXT("\n\n     THREAD ID      = 0x%08X"), te32.th32ThreadID);
					_tprintf(TEXT("\n     Base priority  = %d"), te32.tpBasePri);
					_tprintf(TEXT("\n     Delta priority = %d"), te32.cntUsage);
					_tprintf(TEXT("\n"));
				}
			} while (Thread32Next(hThreadSnap, &te32));

			CloseHandle(hThreadSnap);
			return(TRUE);
		}

		void printError(TCHAR* msg)
		{
			DWORD eNum;
			TCHAR sysMsg[256];
			TCHAR* p;

			eNum = GetLastError();
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, eNum,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				sysMsg, 256, NULL);

			// Trim the end of the line and terminate it with a null
			p = sysMsg;
			while ((*p > 31) || (*p == 9))
				++p;
			do { *p-- = 0; } while ((p >= sysMsg) &&
				((*p == '.') || (*p < 33)));

			// Display the message
			_tprintf(TEXT("\n  WARNING: %s failed with error %d (%s)"), msg, eNum, sysMsg);
		}

	};

	vector<unsigned> pids() {
		vector<unsigned> pid;
		HANDLE hProcessSnap;
		PROCESSENTRY32 pe32;

		hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hProcessSnap == INVALID_HANDLE_VALUE)
		{
			Functions::printError(TEXT("CreateToolhelp32Snapshot (of processes)"));
		}

		pe32.dwSize = sizeof(PROCESSENTRY32);

		if (!Process32First(hProcessSnap, &pe32))
		{
			Functions::printError(TEXT("Process32First")); // show cause of failure
			CloseHandle(hProcessSnap);          // clean the snapshot object
		}

		do
		{
			pid.push_back(pe32.th32ProcessID);

		} while (Process32Next(hProcessSnap, &pe32));

		CloseHandle(hProcessSnap);
		return pid;
	}

	struct CPU {

		map<string, double> cpu_times() {
			FILETIME idle, system, user;
			map<string, double> data;
			if (GetSystemTimes(&idle, &system, &user))
			{
				data["idle"] = Functions::filetime2int(idle);
				data["kernel"] = Functions::filetime2int(system);
				data["user"] = Functions::filetime2int(user);
			}
			return data;
		}

		map<string, double> lastCpuTime;
		double cpu_times_percent(int delay = 1000) {
			//http://www.codeproject.com/Articles/9113/Get-CPU-Usage-with-GetSystemTimes

			map<string, double> CpuTime;

			if (lastCpuTime.size() == 0)
				lastCpuTime = cpu_times();

			Sleep(delay);
			CpuTime = cpu_times();

			double usr = CpuTime["user"] - lastCpuTime["user"];
			double ker = CpuTime["kernel"] - lastCpuTime["kernel"];
			double idle = CpuTime["idle"] - lastCpuTime["idle"];

			double sys = ker + usr;

			double percent = ((sys - idle) / sys)*100.0;

			lastCpuTime = CpuTime;
			return percent;
		}

		unsigned cpu_count(){
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			return si.dwNumberOfProcessors;
		}
	};

	struct Memory {
		map<string, double> virtual_memory() {
			MEMORYSTATUSEX statex;
			statex.dwLength = sizeof(statex);
			GlobalMemoryStatusEx(&statex);
			map<string, double> data;
			data["total"] = (double)statex.ullTotalVirtual;
			data["available"] = (double)statex.ullAvailVirtual;
			data["percent"] = double(data["available"] / data["total"]) * 100;
			data["used"] = double(data["total"] - data["available"]);
			return data;
		}

		map<string, double> paging_memory() {
			MEMORYSTATUSEX statex;
			statex.dwLength = sizeof(statex);
			GlobalMemoryStatusEx(&statex);
			map<string, double> data;
			data["total"] = (double)statex.ullTotalPageFile;
			data["available"] = (double)statex.ullTotalPageFile;
			data["percent"] = (data["available"] / data["total"]) * 100;
			data["used"] = data["total"] - data["available"];
			return data;
		}

		map<string, double> physical_memory() {
			MEMORYSTATUSEX statex;
			statex.dwLength = sizeof(statex);
			GlobalMemoryStatusEx(&statex);
			map<string, double> data;
			data["total"] = (double)statex.ullTotalPhys;
			data["available"] = (double)statex.ullAvailPhys;
			data["percent"] = double(data["available"] / data["total"]) * 100;
			data["used"] = double(data["total"] - data["available"]);
			return data;
		}
	};

	struct Network {
		//http://www.rohitab.com/discuss/topic/40685-how-to-get-list-of-processes-using-internet/
	private:
		string getStatus(DWORD tableStatus) {
			switch (tableStatus) {
			case MIB_TCP_STATE_CLOSED:
				return "CLOSED";
				break;
			case MIB_TCP_STATE_LISTEN:
				return "LISTEN";
				break;
			case MIB_TCP_STATE_SYN_SENT:
				return "SYN-SENT";
				break;
			case MIB_TCP_STATE_SYN_RCVD:
				return "SYN-RECEIVED";
				break;
			case MIB_TCP_STATE_ESTAB:
				return "ESTABLISHED";
				break;
			case MIB_TCP_STATE_FIN_WAIT1:
				return "FIN-WAIT-1";
				break;
			case MIB_TCP_STATE_FIN_WAIT2:
				return "FIN-WAIT-2";
				break;
			case MIB_TCP_STATE_CLOSE_WAIT:
				return "CLOSE-WAIT";
				break;
			case MIB_TCP_STATE_CLOSING:
				return "CLOSING";
				break;
			case MIB_TCP_STATE_LAST_ACK:
				return "LAST-ACK";
				break;
			case MIB_TCP_STATE_TIME_WAIT:
				return "TIME-WAIT";
				break;
			case MIB_TCP_STATE_DELETE_TCB:
				return "DELETE-TCB";
				break;
			default:
				return "UNKNOWN dwState value";
				break;
			}
		}

		string getOffLoadStatus(DWORD status) {
			switch (status) {
			case TcpConnectionOffloadStateInHost:
				return "Owned by the network stack and not offloaded \n";
				break;
			case TcpConnectionOffloadStateOffloading:
				return "In the process of being offloaded\n";
				break;
			case TcpConnectionOffloadStateOffloaded:
				return "Offloaded to the network interface control\n";
				break;
			case TcpConnectionOffloadStateUploading:
				return "In the process of being uploaded back to the network stack \n";
				break;
			default:
				return "UNKNOWN Offload state value\n";
				break;
			}
		}

	public:
		template <typename T>
		void DumpMap(T data) {
			for (auto d : data) {
				for (auto info : d) {
					cout << info.first << ": " << info.second << endl;
				}
				cout << endl;
			}
		}

		NetworkInfo tcpTable(){
			vector<unsigned char> buffer;
			DWORD dwSize = sizeof(MIB_TCPTABLE_OWNER_PID);
			DWORD dwRetValue = 0;
			char szLocalAddr[128];
			char szRemoteAddr[128];

			vector<map<string, string>> data;

			do{
				buffer.resize(dwSize, 0);
				dwRetValue = GetExtendedTcpTable(buffer.data(), &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
			} while (dwRetValue == ERROR_INSUFFICIENT_BUFFER);
			if (dwRetValue == ERROR_SUCCESS)
			{
				PMIB_TCPTABLE_OWNER_PID ptTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());

				in_addr IpAddr;

				for (DWORD i = 0; i < ptTable->dwNumEntries; i++){
					map<string, string> temp;

					IpAddr.S_un.S_addr = ptTable->table[i].dwLocalAddr;
					inet_ntop(AF_INET, &IpAddr, szLocalAddr, sizeof(szLocalAddr));
					IpAddr.S_un.S_addr = ptTable->table[i].dwRemoteAddr;
					inet_ntop(AF_INET, &IpAddr, szRemoteAddr, sizeof(szRemoteAddr));

					temp["type"] = "SOCK_STREAM";
					temp["family"] = "AF_NET";
					temp["laddr"] = szLocalAddr;
					temp["laddrport"] = to_string(htons((unsigned short)ptTable->table[i].dwLocalPort));
					temp["raddr"] = szRemoteAddr;
					temp["raddrport"] = to_string(htons((unsigned short)ptTable->table[i].dwRemotePort));
					temp["status"] = getStatus(ptTable->table[i].dwState);
					temp["pid"] = to_string(ptTable->table[i].dwOwningPid);

					data.push_back(temp);

				}
			}
			return data;
		}

		NetworkInfo tcp6Table(){
			vector<unsigned char> buffer;
			DWORD dwSize = sizeof(MIB_TCP6TABLE_OWNER_PID);
			DWORD dwRetValue = 0;
			char szLocalAddr[128];
			char szRemoteAddr[128];

			vector<map<string, string>> data;

			do{
				buffer.resize(dwSize, 0);
				dwRetValue = GetExtendedTcpTable(buffer.data(), &dwSize, TRUE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
			} while (dwRetValue == ERROR_INSUFFICIENT_BUFFER);
			if (dwRetValue == ERROR_SUCCESS)
			{
				PMIB_TCP6TABLE_OWNER_PID ptTable = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buffer.data());

				in_addr IpAddr;

				for (DWORD i = 0; i < ptTable->dwNumEntries; i++){
					map<string, string> temp;

					IpAddr.S_un.S_addr = (ULONG)ptTable->table[i].ucLocalAddr;
					inet_ntop(AF_INET6, &IpAddr, szLocalAddr, sizeof(szLocalAddr));
					IpAddr.S_un.S_addr = (ULONG)ptTable->table[i].ucRemoteAddr;
					inet_ntop(AF_INET6, &IpAddr, szRemoteAddr, sizeof(szRemoteAddr));

					temp["type"] = "SOCK_STREAM";
					temp["family"] = "AF_NET6";
					temp["laddr"] = szLocalAddr;
					temp["laddrport"] = to_string(htons((unsigned short)ptTable->table[i].dwLocalPort));
					temp["raddr"] = szRemoteAddr;
					temp["raddrport"] = to_string(htons((unsigned short)ptTable->table[i].dwRemotePort));
					temp["status"] = getStatus(ptTable->table[i].dwState);
					temp["pid"] = to_string(ptTable->table[i].dwOwningPid);

					data.push_back(temp);

				}
			}
			return data;
		}

		NetworkInfo udpTable(){
			vector<unsigned char> buffer;
			DWORD dwSize = sizeof(MIB_UDPTABLE_OWNER_PID);
			DWORD dwRetValue = 0;
			char szLocalAddr[128];

			vector<map<string, string>> data;

			do{
				buffer.resize(dwSize, 0);
				dwRetValue = GetExtendedUdpTable(buffer.data(), &dwSize, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
			} while (dwRetValue == ERROR_INSUFFICIENT_BUFFER);
			if (dwRetValue == ERROR_SUCCESS)
			{
				PMIB_UDPTABLE_OWNER_PID ptTable = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buffer.data());

				in_addr IpAddr;

				for (DWORD i = 0; i < ptTable->dwNumEntries; i++){
					map<string, string> temp;

					IpAddr.S_un.S_addr = ptTable->table[i].dwLocalAddr;
					inet_ntop(AF_INET, &IpAddr, szLocalAddr, sizeof(szLocalAddr));

					temp["type"] = "SOCK_DGRAM";
					temp["family"] = "AF_NET";
					temp["laddr"] = szLocalAddr;
					temp["laddrport"] = to_string(htons((unsigned short)ptTable->table[i].dwLocalPort));
					temp["pid"] = to_string(ptTable->table[i].dwOwningPid);

					data.push_back(temp);

				}
			}
			return data;
		}

		NetworkInfo udp6Table(){
			vector<unsigned char> buffer;
			DWORD dwSize = sizeof(MIB_UDP6TABLE_OWNER_PID);
			DWORD dwRetValue = 0;
			char szLocalAddr[128];
			in_addr IpAddr;

			vector<map<string, string>> data;

			do{
				buffer.resize(dwSize, 0);
				dwRetValue = GetExtendedUdpTable(buffer.data(), &dwSize, TRUE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
			} while (dwRetValue == ERROR_INSUFFICIENT_BUFFER);
			if (dwRetValue == ERROR_SUCCESS)
			{
				PMIB_UDP6TABLE_OWNER_PID ptTable = reinterpret_cast<PMIB_UDP6TABLE_OWNER_PID>(buffer.data());

				for (DWORD i = 0; i < ptTable->dwNumEntries; i++){
					map<string, string> temp;

					IpAddr.S_un.S_addr = (ULONG)ptTable->table[i].ucLocalAddr;
					inet_ntop(AF_INET6, &IpAddr, szLocalAddr, sizeof(szLocalAddr));

					temp["type"] = "SOCK_DGRAM";
					temp["family"] = "AF_NET6";
					temp["laddr"] = szLocalAddr;
					temp["laddrport"] = to_string(htons((unsigned short)ptTable->table[i].dwLocalPort));
					temp["pid"] = to_string(ptTable->table[i].dwOwningPid);

					data.push_back(temp);

				}
			}
			return data;
		}

		NetworkInfo allConnections() {
			NetworkInfo tcp, tcp6, udp, udp6, allInfo;
			tcp = tcpTable();
			tcp6 = tcp6Table();
			udp = udpTable();
			udp6 = udp6Table();

			allInfo.reserve(tcp.size() + tcp6.size() + udp.size() + udp6.size());
			allInfo.insert(allInfo.end(), tcp.begin(), tcp.end());
			allInfo.insert(allInfo.end(), tcp6.begin(), tcp6.end());
			allInfo.insert(allInfo.end(), udp.begin(), udp.end());
			allInfo.insert(allInfo.end(), udp6.begin(), udp6.end());

			return allInfo;
		}

		NetworkInfo connections(int id = NULL) {
			string pid = to_string(id);
			NetworkInfo data = allConnections();

			if (id != NULL) {
				int i = 0;
				while (i < (int)data.size()) {
					if (data[i]["pid"] != pid)
						data.erase(data.begin() + i);
					else
						i++;
				}
			}
			return data;
		}

	};

	struct Process {
		HANDLE hProc;
		PROCESSENTRY32 pe32;
		int pid;

		Process(int id) {
			pid = id;
			HANDLE hProcessSnap;
			hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (hProcessSnap == INVALID_HANDLE_VALUE)
				Functions::printError(TEXT("CreateToolhelp32Snapshot (of processes)"));
			pe32.dwSize = sizeof(PROCESSENTRY32);
			if (!Process32First(hProcessSnap, &pe32))
			{
				Functions::printError(TEXT("Process32First")); // show cause of failure
				CloseHandle(hProcessSnap);          // clean the snapshot object
			}

			do
			{
				if (pid == pe32.th32ProcessID) {
					CloseHandle(hProcessSnap);
					break;
				}
			} while (Process32Next(hProcessSnap, &pe32));

			CloseHandle(hProcessSnap);

			/*
			HANDLE hToken;
			LUID luid;
			TOKEN_PRIVILEGES tkp;

			OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
			LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid);
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Luid = luid;
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

			AdjustTokenPrivileges(hToken, false, &tkp, sizeof(tkp), NULL, NULL);
			CloseHandle(hToken);
			*/

			if (id != 0) {
				hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, id);
				//if (hProc == NULL)
				//	Functions::printError(TEXT("OpenProcess"));
			}
		}

		string name() {
			wstring processName;
			processName = pe32.szExeFile;
			string ps = string(processName.begin(), processName.end());
			return ps;
		}

		string path() {
			LPWSTR path = new WCHAR[MAX_PATH];
			DWORD charsCarried = MAX_PATH;
			if (QueryFullProcessImageName(hProc, 0, path, &charsCarried))
			{
				wstring npath = path;
				return string(npath.begin(), npath.end());
			}
			return string();
		}

		unsigned priority() {
			MEMORY_PRIORITY_INFORMATION MemPrio;
			if (GetProcessInformation(hProc,
				ProcessMemoryPriority,
				&MemPrio,
				sizeof(MemPrio))
				)
			{
				return MemPrio.MemoryPriority;
			}
		}

		map<string, double> io_counters() {
			map<string, double> info;
			IO_COUNTERS counter;
			if (GetProcessIoCounters(hProc, &counter)) {
				info["ReadOps"] = (double)counter.ReadOperationCount;
				info["WriteOps"] = (double)counter.WriteOperationCount;
				info["ReadTransfer"] = (double)counter.ReadTransferCount;
				info["WriteTransfer"] = (double)counter.WriteTransferCount;
				info["OtherTransfer"] = (double)counter.OtherTransferCount;
				info["OtherOps"] = (double)counter.OtherOperationCount;
			}
			return info;
		}

		double create_times() {
			FILETIME creation, exit, kernel, user;
			map<string, __int64> data;
			if (GetProcessTimes(hProc, &creation, &exit, &kernel, &user))
			{
				return Functions::filetime2int(creation);
			}
			return double();
		}

		map<string, double> cpu_times() {
			FILETIME creation, exit, kernel, user;
			map<string, double> data;
			if (GetProcessTimes(hProc, &creation, &exit, &kernel, &user))
			{
				data["kernel"] = Functions::filetime2int(kernel);
				data["user"] = Functions::filetime2int(user);
				return data;
			}
			return data;
		}

		map<string, unsigned> memory_info(){
			map<string, unsigned> data;
			PROCESS_MEMORY_COUNTERS memCounter;
			if (GetProcessMemoryInfo(GetCurrentProcess(),
				&memCounter,
				sizeof(memCounter))
				)
			{
				data["vms"] = memCounter.PagefileUsage;
				data["rss"] = memCounter.WorkingSetSize;
				return data;
			}
			return map<string, unsigned>();
		}

		unsigned num_threads() {
			return pe32.cntThreads;
		}

		unsigned parent_id() {
			return pe32.th32ParentProcessID;
		}


		unsigned is64bitProcess() {
			BOOL info;
			if (IsWow64Process(hProc, &info))
			{
				return info;
			}
			return 0;
		}

		map<string, double> lastCpuTime, lastProcessTime;
		double cpu_times_percent(int delay = 1000) {
			//http://www.philosophicalgeek.com/2009/01/03/determine-cpu-usage-of-current-process-c-and-c/
			map<string, double> CpuTime, ProcessTime;
			double usr, ker, sys, proc, percent;

			if (lastCpuTime.size() == 0) {
				CPU c;
				lastCpuTime = c.cpu_times();
				lastProcessTime = cpu_times();
			}

			Sleep(delay);
			CpuTime = cpu_times();

			usr = CpuTime["user"] - lastCpuTime["user"];
			ker = CpuTime["kernel"] - lastCpuTime["kernel"];
			sys = ker + usr;

			usr = ProcessTime["user"] - lastProcessTime["user"];
			ker = ProcessTime["kernel"] - lastProcessTime["kernel"];
			proc = ker + usr;

			percent = (proc / sys) * 100;

			lastCpuTime = CpuTime;
			lastProcessTime = ProcessTime;

			return percent;
		}

		NetworkInfo connections(int pid) {
			psutilpp::Network n;
			return n.connections(pid);
		}

	};
};

#endif