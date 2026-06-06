#include "MskWindow/MskWindow.h"


int main ()
{
	MskWindowInit();
	MskWindow win;
	win.mainLoop();
    return 0;
}