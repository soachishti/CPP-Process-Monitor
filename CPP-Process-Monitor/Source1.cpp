//#include "psutilpp.h"
//#include <iomanip>
//
//void main() {
//	
//	auto data = psutilpp::pids();
//	for (auto d : data) {
//		cout << d << endl;
//	}
//
//	auto data = psutilpp::pids();
//
//	wcout << left
//		<< setw(6) << "ID"
//		<< setw(20) << "ProcessName" 
//		<< setw(10) << "Usage %"
//		<< setw(10) << "Read/s"
//		<< setw(10) << "Write/s"
//		<< setw(10) << "Mem"
//		<< setw(10) << "Virtual"
//		<< endl;
//
//	while (1) {
//		for (auto id : data) {
//			try {
//				auto p = psutilpp::Process(id);
//				map<string, double> io = p.io_counters();
//				map<string, double> mem = p.memory_info();
//
//				string psName = string(p.processName.begin(), p.processName.end());
//				psName.resize(20);
//
//				cout << fixed << setprecision(2) << left
//					<< setw(6) << p.pid // id
//					<< setw(20) << psName // process name
//					<< setw(10) << p.cpu_times_percent(0)
//					<< setw(10) << io["ReadTransfer"] / 1024
//					<< setw(10) << io["WriteTransfer"] / 1024
//					<< setw(10) << (mem["rss"] / (1024 * 1024))
//					<< setw(10) << (mem["vms"] / (1024 * 1024))
//					<< endl;
//			}
//			catch (CPdhQuery::CException const &e)
//			{
//				tcout << e.What() << std::endl;
//			}
//		}
//		Sleep(1000);
//	}
//}