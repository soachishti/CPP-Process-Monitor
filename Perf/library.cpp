#ifndef LIBRARY_H
#define LIBRARY_H

#include <iostream>
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>
#include <sstream>
#include <vector>
#include <tchar.h>
#include <vector>
#include <tuple>
#include <map>
#include <cmath>

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")
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

#ifdef UNICODE
#define tcout std::wcout
#else
#define tcout std::cout
#endif

using namespace std;

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

inline void DumpMap(std::map<std::tstring, double> const &m)
{
	std::map<std::tstring, double>::const_iterator itr = m.begin();
	while (m.end() != itr)
	{
		tcout << itr->first << " " << itr->second << endl;
		++itr;
	}
}

inline double DumpMapTotal(std::map<std::tstring, double> const &m)
{
	std::map<std::tstring, double>::const_iterator itr = m.begin();
	double count = 0;
	while (m.end() != itr)
	{
		count += itr->second;
		++itr;
	}
	return count;
}

inline void gotoxy(int column, int line)
{
	COORD coord;
	coord.X = column;
	coord.Y = line;
	SetConsoleCursorPosition(
		GetStdHandle(STD_OUTPUT_HANDLE),
		coord
		);
}

inline vector<tuple<wstring, wstring, wstring, wstring>> listAllDataCorrect() {
	HRESULT hres;

	hres = CoInitializeEx(0, COINIT_MULTITHREADED);

	hres = CoInitializeSecurity(
		NULL,
		-1,                          // COM authentication
		NULL,                        // Authentication services
		NULL,                        // Reserved
		RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
		RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
		NULL,                        // Authentication info
		EOAC_NONE,                   // Additional capabilities 
		NULL                         // Reserved
		);

	IWbemLocator *pLoc = NULL;

	hres = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID *)&pLoc);

	IWbemServices *pSvc = NULL;

	hres = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
		NULL,                    // User name. NULL = current user
		NULL,                    // User password. NULL = current
		0,                       // Locale. NULL indicates current
		NULL,                    // Security flags.
		0,                       // Authority (for example, Kerberos)
		0,                       // Context object 
		&pSvc                    // pointer to IWbemServices proxy
		);

	//cout << "Connected to ROOT\\CIMV2 WMI namespace" << endl;

	hres = CoSetProxyBlanket(
		pSvc,                        // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
		NULL,                        // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE                    // proxy capabilities 
		);

	IEnumWbemClassObject* pEnumerator = NULL;
	hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_PerfFormattedData_PerfProc_Process"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	IWbemClassObject *pclsObj = NULL;
	ULONG uReturn = 0;

	vector<tuple<wstring, wstring, wstring, wstring>> finalData;

	while (pEnumerator)
	{
		pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			break;
		}

		VARIANT vtProp;

		// Get the value of the Name property
		pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		wstring processName = vtProp.bstrVal;

		if (
			processName.find(L"_Total") != wstring::npos
			|| processName.find(L"Idle") != wstring::npos
			) {
			continue;
		}

		pclsObj->Get(L"PercentUserTime", 0, &vtProp, 0, 0);
		wstring processUsage = vtProp.bstrVal;

		pclsObj->Get(L"IOReadBytesPerSec", 0, &vtProp, 0, 0);
		wstring read = vtProp.bstrVal;

		pclsObj->Get(L"IOWriteBytesPerSec", 0, &vtProp, 0, 0);
		wstring write = vtProp.bstrVal;

		finalData.push_back(make_tuple(processName, processUsage, read, write));

		VariantClear(&vtProp);

		pclsObj->Release();
	}

	pSvc->Release();
	pLoc->Release();
	pEnumerator->Release();
	CoUninitialize();

	return finalData;
}

#endif