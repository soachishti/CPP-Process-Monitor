#ifndef PDHQUERY_H
#define PDHQUERY_H

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

#endif PDHQUERY_H