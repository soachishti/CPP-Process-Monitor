/*
Source: https://askldjd.wordpress.com/2011/01/05/a-pdh-helper-class-cpdhquery/
*/

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <tchar.h>
#include <iostream>

#include <string>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "pdh.lib")

using namespace std;

namespace std
{
	typedef std::basic_string<TCHAR> tstring;
	typedef std::basic_ostream<TCHAR> tostream;
	typedef std::basic_istream<TCHAR> tistream;
	typedef std::basic_ostringstream<TCHAR> tostringstream;
	typedef std::basic_istringstream<TCHAR> tistringstream;
	typedef std::basic_stringstream<TCHAR> tstringstream;
} // end namespace

#ifdef UNICODE
#define tcout std::wcout
#else
#define tcout std::cout
#endif

class CPdhQuery
{
public:

	// Inner exception class to report error.
	class CException
	{
	public:
		CException(std::tstring const & errorMsg) : m_errorMsg(errorMsg)    {}
		std::tstring What() const { return m_errorMsg; }
	private:
		std::tstring m_errorMsg;
	};

	//! Constructor
	explicit CPdhQuery(std::tstring const &counterPath)
		: m_pdhQuery(NULL)
		, m_pdhStatus(ERROR_SUCCESS)
		, m_pdhCounter(NULL)
		, m_counterPath(counterPath)
	{
		if (m_pdhStatus = PdhOpenQuery(NULL, 0, &m_pdhQuery))
		{
			throw CException(GetErrorString(m_pdhStatus));
		}

		// Specify a counter object with a wildcard for the instance.
		if (m_pdhStatus = PdhAddCounter(
			m_pdhQuery,
			m_counterPath.c_str(),
			0,
			&m_pdhCounter)
			)
		{
			GetErrorString(m_pdhStatus);
			throw CException(GetErrorString(m_pdhStatus));
		}
	}

	//! Destructor. The counter and query handle will be closed.
	~CPdhQuery()
	{
		m_pdhCounter = NULL;
		if (m_pdhQuery)
			PdhCloseQuery(m_pdhQuery);
	}

	//! Collect all the data since the last sampling period.
	std::map<std::tstring, double> CollectQueryData()
	{
		std::map<std::tstring, double> collectedData;

		while (true)
		{
			// Collect the sampling data. This might cause
			// PdhGetFormattedCounterArray to fail because some query type
			// requires two collections (or more?). If such scenario is
			// detected, the while loop will retry.
			if (m_pdhStatus = PdhCollectQueryData(m_pdhQuery))
			{
				throw CException(GetErrorString(m_pdhStatus));
			}

			// Size of the pItems buffer
			DWORD bufferSize = 0;

			// Number of items in the pItems buffer
			DWORD itemCount = 0;

			PDH_FMT_COUNTERVALUE_ITEM *pdhItems = NULL;

			// Call PdhGetFormattedCounterArray once to retrieve the buffer
			// size and item count. As long as the buffer size is zero, this
			// function should return PDH_MORE_DATA with the appropriate
			// buffer size.
			m_pdhStatus = PdhGetFormattedCounterArray(
				m_pdhCounter,
				PDH_FMT_DOUBLE,
				&bufferSize,
				&itemCount,
				pdhItems);

			// If the returned value is nto PDH_MORE_DATA, the function
			// has failed.
			if (PDH_MORE_DATA != m_pdhStatus)
			{
				throw CException(GetErrorString(m_pdhStatus));
			}

			std::vector<unsigned char> buffer(bufferSize);
			pdhItems = (PDH_FMT_COUNTERVALUE_ITEM *)(&buffer[0]);

			m_pdhStatus = PdhGetFormattedCounterArray(
				m_pdhCounter,
				PDH_FMT_DOUBLE,
				&bufferSize,
				&itemCount,
				pdhItems);

			if (ERROR_SUCCESS != m_pdhStatus)
			{
				continue;
			}

			tstring name;
			// Everything is good, mine the data.
			for (DWORD i = 0; i < itemCount; i++)			{
				name = tstring(pdhItems[i].szName);

				// Add no after duplicate processes eg chrome, chrome#2
				map<std::tstring, double>::iterator it;
				it = collectedData.find(name);
				u_int count = 1;
				string appData = "";
				wstring tmp;
				while (it != collectedData.end()) {
					appData = "#" + to_string(count);
					count++;
					tmp = tstring(appData.begin(), appData.end());
					it = collectedData.find(name + tmp);
				}

				collectedData.insert(
					std::make_pair(
					std::tstring(pdhItems[i].szName + tmp),
					pdhItems[i].FmtValue.doubleValue)
					);
			}

			pdhItems = NULL;
			bufferSize = itemCount = 0;
			break;
		}
		return collectedData;
	}

private:
	//! Helper function that translate the PDH error code into
	//! an useful message.
	std::tstring GetErrorString(PDH_STATUS errorCode)
	{
		HANDLE hPdhLibrary = NULL;
		LPTSTR pMessage = NULL;
		DWORD_PTR pArgs[] = { (DWORD_PTR)m_searchInstance.c_str() };
		std::tstring errorString;

		hPdhLibrary = LoadLibrary(_T("pdh.dll"));
		if (NULL == hPdhLibrary)
		{
			std::tstringstream ss;
			ss
				<< _T("Format message failed with ")
				<< std::hex << GetLastError();
			return ss.str();
		}

		if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE |
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			/*FORMAT_MESSAGE_IGNORE_INSERTS |*/
			FORMAT_MESSAGE_ARGUMENT_ARRAY,
			hPdhLibrary,
			errorCode,
			0,
			(LPTSTR)&pMessage,
			0,
			(va_list*)pArgs))
		{
			std::tstringstream ss;
			ss
				<< m_counterPath
				<< _T(" ")
				<< _T("Format message failed with ")
				<< std::hex
				<< GetLastError()
				<< std::endl;
			errorString = ss.str();
		}
		else
		{
			errorString += m_counterPath;
			errorString += _T(" ");
			errorString += pMessage;
			LocalFree(pMessage);
		}

		return errorString;
	}

private:
	PDH_HQUERY m_pdhQuery;
	PDH_STATUS m_pdhStatus;
	PDH_HCOUNTER m_pdhCounter;
	std::tstring m_searchInstance;
	std::tstring m_counterPath;
};

class getProcessCpuUsage {
public:
	ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
	int numProcessors;
	HANDLE self;

	~getProcessCpuUsage() {
		CloseHandle(self);
	}

	getProcessCpuUsage(DWORD procId) {
		self = OpenProcess(PROCESS_ALL_ACCESS, TRUE, procId);

		SYSTEM_INFO sysInfo;
		FILETIME ftime, fsys, fuser;

		GetSystemInfo(&sysInfo);
		numProcessors = sysInfo.dwNumberOfProcessors;


		GetSystemTimeAsFileTime(&ftime);
		memcpy(&lastCPU, &ftime, sizeof(FILETIME));

		GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
		memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
		memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
	}

	double get(){
		FILETIME ftime, fsys, fuser;
		ULARGE_INTEGER now, sys, user;
		double percent;

		GetSystemTimeAsFileTime(&ftime);
		memcpy(&now, &ftime, sizeof(FILETIME));

		GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
		memcpy(&sys, &fsys, sizeof(FILETIME));
		memcpy(&user, &fuser, sizeof(FILETIME));
		percent = double(sys.QuadPart - lastSysCPU.QuadPart) + double(user.QuadPart - lastUserCPU.QuadPart);
		percent /= (now.QuadPart - lastCPU.QuadPart);
		percent /= numProcessors;
		lastCPU = now;
		lastUserCPU = user;
		lastSysCPU = sys;
		percent *= 100;

		if (isnan(percent) || isinf(percent)) return 0;

		return percent;
	}
};

void DumpMap(std::map<std::tstring, double> const &m)
{
	std::map<std::tstring, double>::const_iterator itr = m.begin();
	while (m.end() != itr)
	{
		tcout << itr->first << " " << itr->second << std::endl;
		++itr;
	}
}

void gotoxy(int column, int line)
{
	COORD coord;
	coord.X = column;
	coord.Y = line;
	SetConsoleCursorPosition(
		GetStdHandle(STD_OUTPUT_HANDLE),
		coord
		);
}

double DumpMapTotal(map<std::tstring, double> const &m)
{
	double counter = 0;
	std::map<std::tstring, double>::const_iterator itr = m.begin();
	while (m.end() != itr)
	{
		counter += itr->second;
		++itr;
	}
	return counter;
}

using namespace std;
void main()
{
	double SumCpuUsage = 0;
	gotoxy(32, 10);
	cout << "Loading...";

	try
	{
		CPdhQuery pdhQuery(tstring(_T("\\Process(*)\\ID Process")));
		CPdhQuery pdhQueryCpuUsage(tstring(_T("\\Processor Information(_Total)\\% Processor Time")));
		CPdhQuery pdhQueryDownload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));
		CPdhQuery pdhQueryUpload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));

		map<tstring, tuple<getProcessCpuUsage*, CPdhQuery*, CPdhQuery*, CPdhQuery*>> queries;

		while (true) {
			map<tstring, double> data = pdhQuery.CollectQueryData();
			for (map<tstring, double>::iterator itr = data.begin(); itr != data.end(); ++itr)
			{
				if (itr->first == L"_Total" || itr->first == L"Idle") {
					itr++;
					continue;
				}

				if (queries.find(itr->first) == queries.end()) {

					getProcessCpuUsage *q1 = new getProcessCpuUsage((DWORD)itr->second);
					q1->get();

					CPdhQuery *q2 = new CPdhQuery(L"\\Process(" + itr->first + L")\\IO Read Bytes/sec");
					CPdhQuery *q3 = new CPdhQuery(L"\\Process(" + itr->first + L")\\IO Write Bytes/sec");
					CPdhQuery *q4 = new CPdhQuery(L"\\Process(" + itr->first + L")\\% Processor Time");

					queries[itr->first] = make_tuple(q1, q2, q3, q4);
				}
			}

			ostringstream oss;
			vector<tuple<string, double, double, double>> finalData;

			SumCpuUsage = 0;
			for (map<tstring, tuple<getProcessCpuUsage*, CPdhQuery*, CPdhQuery*, CPdhQuery*>>::iterator it = queries.begin();
				it != queries.end(); ++it) {
				try {
					string processName = string(it->first.begin(), it->first.end());
					processName.resize(20);


					double cpuUsage = get<0>(it->second)->get();
					double tmpCpuUsage = DumpMapTotal(get<3>(it->second)->CollectQueryData());

					if (tmpCpuUsage != 100)
						cpuUsage = tmpCpuUsage;

					SumCpuUsage += cpuUsage;

					double read = DumpMapTotal(get<1>(it->second)->CollectQueryData());
					double write = DumpMapTotal(get<2>(it->second)->CollectQueryData());

					finalData.push_back(make_tuple(processName, cpuUsage, read, write));
				}
				catch (CPdhQuery::CException const &e)
				{
					//If process terminates
					e;
					delete get<0>(it->second);
					delete get<1>(it->second);
					delete get<2>(it->second);
					queries.erase(it++);
				}

			}

			sort(finalData.begin(), finalData.end(),
				[](tuple<string, double, double, double> a, tuple<string, double, double, double> b) {
				return (get<1>(a) > get<1>(b));
			}
			);

			double tmpCpuUsage = DumpMapTotal(pdhQueryCpuUsage.CollectQueryData());
			if (tmpCpuUsage == 100 && SumCpuUsage < 100) {
				tmpCpuUsage = SumCpuUsage;
			}

			oss << fixed << "Total CPU Usage: " << setprecision(1) << SumCpuUsage << "%"
				<< endl
				<< "Total Network Usage (DOWN): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryDownload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
				<< "Total Network Usage (UP): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryUpload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
				<< endl

				<< setw(20) << left << "Process Name"
				<< right
				<< setw(19) << "CPU Usage"
				<< setw(19) << "Sent"
				<< setw(19) << "Received" << endl;


			for (auto d : finalData) {
				oss << left << setw(20) << get<0>(d)
					<< right << setprecision(1) << fixed
					<< setw(15) << get<1>(d) << " %"
					<< setw(15) << get<2>(d) / (1024 * 8) << " Kb/s"
					<< setw(15) << get<3>(d) / (1024 * 8) << " Kb/s" << endl;
			}
			system("cls");
			cout << oss.str();
			gotoxy(0, 0);
			oss.str("");
			Sleep(1500);
		}
	}
	catch (CPdhQuery::CException const &e)
	{
		tcout << e.What() << std::endl;
	}
}