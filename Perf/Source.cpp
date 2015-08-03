#include "library.cpp"

#include <iostream>
#include <iomanip>
#include <windows.h>
#include <sstream>

using namespace std;

int main() {
	try {
		gotoxy(33, 10);
		cout << "Loading...";

		CPdhQuery pdhQueryCpuUsage(tstring(_T("\\Processor Information(_Total)\\% Processor Time")));
		CPdhQuery pdhQueryDownload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));
		CPdhQuery pdhQueryUpload(tstring(_T("\\Network Interface(*)\\Bytes Received/sec")));

		vector<tuple<wstring, wstring, wstring, wstring>> data = listAllDataCorrect();

		system("cls");

		while (true) {
			ostringstream ss;

			ss << "Total CPU Usage: " << setprecision(3) << DumpMapTotal(pdhQueryCpuUsage.CollectQueryData()) << "%"
				<< endl
				<< "Total Network Usage (DOWN): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryDownload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
				<< "Total Network Usage (UP): " << fixed << setprecision(2) << DumpMapTotal(pdhQueryUpload.CollectQueryData()) / (1024 * 8) << " Kbps" << endl
				<< endl;

			ss << setw(20) << left << "Process Name"
				<< right
				<< setw(19) << "CPU Usage"
				<< setw(20) << "Sent"
				<< setw(20) << "Received" << endl;

			for (auto n : data) {

				string processName = string(get<0>(n).begin(),get<0>(n).end());
				processName.resize(20, ' ');

				string processUsage = string(get<1>(n).begin(), get<1>(n).end());
				double read = stoi(string(get<2>(n).begin(), get<2>(n).end())) / (1024*8);
				double write = stoi(string(get<3>(n).begin(), get<3>(n).end())) / (1024 * 8);

				ss << left
					<< setw(20) << processName
					<< fixed
					<< right
					<< setw(18) << processUsage << "%"
					<< setprecision(2)
					<< setw(15) << read << " Kbps"
					<< setw(15) << write << " Kbps"
					<< endl;

			}
			cout << ss.str();

			gotoxy(0, 0);
			Sleep(1000);
			data = listAllDataCorrect();
			system("cls");
		}
	}
	catch (CPdhQuery::CException const &e)
	{
		e.What();
		exit(0);
	}

}