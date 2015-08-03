#include <iostream>
#include <queue>
#include <thread>
#include <vector>
#include <tuple>
#include <map>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <string>
#include <tchar.h>
#include <cmath>
#include <conio.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <algorithm>

#pragma comment(lib, "pdh.lib")

#define isnan(x) _isnan(x)
#define isinf(x) (!_finite(x))
#define fpu_error(x) (isinf(x) || isnan(x))

namespace std
{
	typedef std::basic_string<TCHAR> tstring;
	typedef std::basic_ostream<TCHAR> tostream;
	typedef std::basic_istream<TCHAR> tistream;
	typedef std::basic_ostringstream<TCHAR> tostringstream;
	typedef std::basic_istringstream<TCHAR> tistringstream;
	typedef std::basic_stringstream<TCHAR> tstringstream;
} // end namespace
using namespace std;

#ifdef UNICODE
#define tcout std::wcout
#else
#define tcout std::cout
#endif

bool threadSignal = false;
bool threadExit = false;
queue<tuple<string, double, double, double>> myQueue;
u_int refreshRate = 1500;


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

	CPdhQuery() {}

	//! Constructor
	explicit CPdhQuery(std::tstring &counterPath)
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
			for (DWORD i = 0; i < itemCount; i++)
			{
				name = tstring(pdhItems[i].szName);

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
					name + tmp,
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
	tstring GetErrorString(PDH_STATUS errorCode)
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
			tstringstream ss;
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

void DumpMap(map<tstring, double> const &m)
{
	map<tstring, double>::const_iterator itr = m.begin();
	while (m.end() != itr)
	{
		tcout << itr->first << " " << itr->second << endl;
		++itr;
	}
}

double DumpMapTotal(map<tstring, double> const &m)
{
	map<tstring, double>::const_iterator itr = m.begin();
	double count = 0;
	while (m.end() != itr)
	{
		count += itr->second;
		++itr;
	}
	return count;
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

void getData(wstring processName) {
	tuple<string, double, double, double> tmp;
	wstring query;

	try {
		query = L"\\Process(" + processName + L")\\% Processor Time";
		CPdhQuery pdhProcessUsage(query);
		pdhProcessUsage.CollectQueryData();

		query = L"\\Process(" + processName + L")\\IO Read Bytes/sec";
		CPdhQuery pdhProcessReadBytes(query);
		pdhProcessReadBytes.CollectQueryData();

		query = L"\\Process(" + processName + L")\\IO Write Bytes/sec";
		CPdhQuery pdhProcessWriteBytes(query);
		pdhProcessWriteBytes.CollectQueryData();
		
		bool status;
		while (threadExit == false) {
			status = threadSignal;
			tmp = make_tuple(
				string(processName.begin(), processName.end()),
				DumpMapTotal(pdhProcessUsage.CollectQueryData()),
				DumpMapTotal(pdhProcessReadBytes.CollectQueryData()),
				DumpMapTotal(pdhProcessWriteBytes.CollectQueryData())
				);
			myQueue.push(tmp);

			while (threadSignal == status) Sleep(10);
		}
		return;
	}
	catch (CPdhQuery::CException const &e)
	{
		e; return;
	}
}

void listAllData()
{
	gotoxy(32, 10);
	cout << "Loading...";

	wstring processName;
	map<wstring, thread*> threads;
	int count = 0;
	tuple<string, double, double, double> tmp;
	vector<tuple<string, double, double, double>> tmpVec;
	ostringstream ss;
	map<tstring, double> m;
	map<tstring, double>::const_iterator itr;

	CPdhQuery pdhProcessDetail(tstring(_T("\\Process(*)\\% Processor Time")));
	CPdhQuery pdhQueryCpuUsage(tstring(_T("\\Processor Information(_Total)\\% Processor Time")));
	CPdhQuery pdhQueryDownload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));
	CPdhQuery pdhQueryUpload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));

	while (!_kbhit()) {
		threadSignal = (threadSignal) ? false : true;

		m = pdhProcessDetail.CollectQueryData();
		itr = m.begin();

		count = 0;
		while (m.end() != itr)
		{
			processName = itr->first;

			if (processName.find(L"_Total") != tstring::npos ||
				processName.find(L"Idle") != tstring::npos
				) {
				++itr;
				continue;
			}

			if (threads.find(processName) == threads.end())
			{
				thread *t = new thread(getData, processName);
				threads[processName] = t;
				//cout << "Thread started " << count << endl;
			}

			count++;
			++itr;
		}

		Sleep(refreshRate);

		tmpVec.clear();
		while (!myQueue.empty()) {
			tmp = myQueue.front();
			tmpVec.push_back(tmp);
			myQueue.pop();
		}
		
		sort(tmpVec.begin(), tmpVec.end(), 
			[](tuple<string, double, double, double> a,	tuple<string, double, double, double> b) {
				return (get<1>(a) > get<1>(b));
			}
		);

		ss << "Total CPU Usage: " << setprecision(3) << DumpMapTotal(pdhQueryCpuUsage.CollectQueryData()) << "%"
			<< endl
			<< "Total Network Usage (DOWN): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryDownload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
			<< "Total Network Usage (UP): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryUpload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
			<< endl
		
			<< setw(20) << left << "Process Name"
			<< right
			<< setw(19) << "CPU Usage"
			<< setw(19) << "Sent"
			<< setw(19) << "Received" << endl << endl;

		for (auto d : tmpVec) {
			string processName = get<0>(d);
			processName.resize(20);

			ss << left
				<< setw(20) << processName
				<< fixed
				<< right
				<< setprecision(1)
				<< setw(16) << get<1>(d) << " %"
				<< setprecision(2)
				<< setw(15) << get<2>(d) / (1024 * 8) << " Kbps"
				<< setw(15) << get<3>(d) / (1024 * 8) << " Kbps"
				<< endl;
		}
		system("cls");

		cout << ss.str();
		ss.str(""); // Cleaning string stream
		
		gotoxy(0,0);
	}

	threadExit = true; // send signal to exit thread loop
	threadSignal = (threadSignal) ? false : true; // Let thread proceed to exit 

	//Cleaning stuff 
	for (map<wstring, thread*>::iterator it = threads.begin(); it != threads.end(); ++it)
	{
		it->second->join();
		delete it->second;
	}
}

int main() {
	listAllData();
}