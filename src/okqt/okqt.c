/* Press Return on a visible top-level Qt dialog with the exact given title (default "Viz Artist"). */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static const char *want; static HWND found;
static BOOL CALLBACK cb(HWND h, LPARAM lp) { char t[256]; (void)lp; if (!IsWindowVisible(h)) return TRUE; GetWindowTextA(h, t, sizeof t); if (strcmp(t, want) == 0) { found = h; return FALSE; } return TRUE; }
int main(int argc, char **argv) {
    want = argc > 1 ? argv[1] : "Viz Artist"; int wait = argc > 2 ? atoi(argv[2]) : 60, i;
    for (i = 0; i < wait * 4; i++) { found = NULL; EnumWindows(cb, 0);
        if (found) { SetForegroundWindow(found); PostMessageA(found, WM_KEYDOWN, VK_RETURN, 0x001C0001); PostMessageA(found, WM_KEYUP, VK_RETURN, 0xC01C0001);
            printf("okqt: sent Return to %p '%s'\n", (void*)found, want); Sleep(800); return 0; }
        Sleep(250); }
    printf("okqt: no window '%s' within %d s\n", want, wait); return 1; }
