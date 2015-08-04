#include <windows.h>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <iostream>
#include <string>
#include <iomanip>
#include <algorithm>

#include "PdhQuery.hpp"
#include "Func.hpp"

using namespace std;

int refreshRate = 1000;

void main()
{
	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	gotoxy(32, 10);
	cout << "Loading...";

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);

	int numCPU = sysinfo.dwNumberOfProcessors;

	try
	{
		CPdhQuery pdhQuery(tstring(_T("\\Process(*)\\% Processor Time")));
		CPdhQuery pdhQueryCpuUsage(tstring(_T("\\Processor Information(_Total)\\% Processor Time")));
		CPdhQuery pdhQueryDownload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));
		CPdhQuery pdhQueryUpload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));

		map<tstring, tuple<CPdhQuery*, CPdhQuery*, CPdhQuery*>> queries;
		int noOfProcesses = 0;
		while (true) {
			map<tstring, double> data = pdhQuery.CollectQueryData();
			noOfProcesses = 0;
			for (map<tstring, double>::iterator itr = data.begin(); itr != data.end(); ++itr)
			{
				if (itr->first == L"_Total" || itr->first == L"Idle") {
					itr++;
					continue;
				}

				if (queries.find(itr->first) == queries.end()) {
					CPdhQuery *q1 = new CPdhQuery(L"\\Process(" + itr->first + L")\\% Processor Time");
					q1->CollectQueryData();
					CPdhQuery *q2 = new CPdhQuery(L"\\Process(" + itr->first + L")\\IO Read Bytes/sec");
					q2->CollectQueryData();
					CPdhQuery *q3 = new CPdhQuery(L"\\Process(" + itr->first + L")\\IO Write Bytes/sec");
					q3->CollectQueryData();
					queries[itr->first] = make_tuple(q1, q2, q3);
					Sleep(5);
				}
				noOfProcesses++;
			}

			ostringstream oss;
			vector<tuple<string, double, double, double>> finalData;

			int timeDelay = refreshRate / noOfProcesses;

			for (map<tstring, tuple<CPdhQuery*, CPdhQuery*, CPdhQuery*>>::iterator it = queries.begin();
				it != queries.end(); ++it) {
				try {
					string processName = string(it->first.begin(), it->first.end());
					processName.resize(20);

					double cpuUsage = DumpMapTotal(get<0>(it->second)->CollectQueryData());
					double read = DumpMapTotal(get<1>(it->second)->CollectQueryData());
					double write = DumpMapTotal(get<2>(it->second)->CollectQueryData());
					finalData.push_back(make_tuple(processName, cpuUsage, read, write));

					Sleep(timeDelay);
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

			oss << fixed << "Total CPU Usage: " << setprecision(1) << DumpMapTotal(pdhQueryCpuUsage.CollectQueryData()) << "%"
				<< endl
				<< "Total Network Usage (DOWN): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryDownload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
				<< "Total Network Usage (UP): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryUpload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
				<< endl

				<< setw(20) << left << "Process Name"
				<< right
				<< setw(19) << "CPU Usage"
				<< setw(19) << "Read"
				<< setw(19) << "Write" << endl;


			for (auto d : finalData) {
				oss << left << setw(20) << get<0>(d)
					<< right << setprecision(1) << fixed
					<< setw(15) << (get<1>(d) / numCPU) << " %"
					<< setw(15) << get<2>(d) / (1024 * 8) << " Kb/s"
					<< setw(15) << get<3>(d) / (1024 * 8) << " Kb/s" << endl;
			}
			system("cls");
			cout << oss.str();
			gotoxy(0, 0);
			oss.str("");
		}
	}
	catch (CPdhQuery::CException const &e)
	{
		tcout << e.What() << std::endl;
	}
}