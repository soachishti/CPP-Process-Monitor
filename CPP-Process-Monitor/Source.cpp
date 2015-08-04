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

	getInformation g(1500); // Delay between query
	while (true) {
		g.printData();
	}
}