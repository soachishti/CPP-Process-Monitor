#include <windows.h>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <iostream>
#include <string>
#include <iomanip>
#include <algorithm>

#include "CPdhQuery.h"
#include "Func.hpp"

#include "GetInfoPerfCount.hpp"

using namespace std;

void main()
{
	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	gotoxy(32, 10);
	cout << "Loading...";

	PerfCount::getInformation g; // Delay between query 1500 (1.5sec)
	while (true) {
		g.printData();
	}
}