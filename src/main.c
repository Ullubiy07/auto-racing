#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "track.h"

void ClearScreen() {
	dprintf(1, "\x1b[2J");    // Очистить весь экран
	dprintf(1, "\x1b[0;0f");  // Переместить курсор в левый верхний угол
}

int main() {
	ClearScreen();
	return 0;
}
