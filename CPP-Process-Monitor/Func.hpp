#ifndef FUNC_H
#define FUNC_H

#include <windows.h>

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

struct getInformation {

	typedef vector<tuple<string, double, double, double>> ProcessDetail;

	map<tstring, tuple<CPdhQuery*, CPdhQuery*, CPdhQuery*>> queries;
	int noOfProcesses = 0;
	int numCPU =0;

	CPdhQuery processName, totalCpuUsage, download, upload;

	int refreshRate = 0;

	getInformation(int rr = 1500) {
		refreshRate = rr;

		processName.init(tstring(_T("\\Process(*)\\% Processor Time")));
		totalCpuUsage.init (tstring(_T("\\Processor Information(_Total)\\% Processor Time")));
		download.init(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));
		upload.init(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));

		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		numCPU = sysinfo.dwNumberOfProcessors;
	}
	
	double getCPUUsage() {
		return DumpMapTotal(totalCpuUsage.CollectQueryData());
	}

	double getDownload() {
		return DumpMapTotal(download.CollectQueryData());
	}

	double getUpload() {
		return DumpMapTotal(download.CollectQueryData());
	}

	void loadProcessAndCounter() {
		map<tstring, double> data = processName.CollectQueryData();
		noOfProcesses = 0;
		for (map<tstring, double>::iterator itr = data.begin(); itr != data.end(); ++itr)
		{
			if (itr->first == L"_Total" || itr->first == L"Idle") {
				itr++;
				continue;
			}

			if (queries.find(itr->first) == queries.end()) {
				CPdhQuery *q1 = new CPdhQuery;
				q1->init(L"\\Process(" + itr->first + L")\\% Processor Time");
				q1->CollectQueryData();

				CPdhQuery *q2 = new CPdhQuery;
				q2->init(L"\\Process(" + itr->first + L")\\IO Read Bytes/sec");
				q2->CollectQueryData();
				
				CPdhQuery *q3 = new CPdhQuery;
				q3->init(L"\\Process(" + itr->first + L")\\IO Write Bytes/sec");
				q3->CollectQueryData();
				
				queries[itr->first] = make_tuple(q1, q2, q3);
				Sleep(5);
			}
			noOfProcesses++;
		}
	}

	ProcessDetail readData() {
		loadProcessAndCounter();
		ProcessDetail finalData;

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


		// Sort data by highest cpu usage
		sort(finalData.begin(), finalData.end(),
				[](tuple<string, double, double, double> a, tuple<string, double, double, double> b) {
				return (get<1>(a) > get<1>(b));
			}
		);
		return finalData;
	}

	void printData() {
		while (true) {
			ostringstream oss;
			oss << fixed << "Total CPU Usage: " << setprecision(1) << getCPUUsage() << "%"
				<< endl
				<< "Total Network Usage (DOWN): " << fixed << setprecision(2) << getDownload() / (1024 * 8) << " Kbps" << endl
				<< "Total Network Usage (UP): " << fixed << setprecision(2) << getUpload() / (1024 * 8) << " Kbps" << endl
				<< endl

				<< setw(20) << left << "Process Name"
				<< right
				<< setw(19) << "CPU Usage"
				<< setw(19) << "Sent"
				<< setw(19) << "Received" << endl;

			ProcessDetail finalData = readData();
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

};

#endif FUNC_H