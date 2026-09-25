/* Dismiss Viz Engine's startup "Failed to remove system menu item CLOSE." message box from inside Wine
 * (no X fake input needed). Waits up to N seconds (arg1, default 60) for the box, presses OK, exits. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static HWND found;
static BOOL CALLBACK enum_cb(HWND h, LPARAM lp) {
    char cls[64], title[128]; (void)lp;
    GetClassNameA(h, cls, sizeof cls); GetWindowTextA(h, title, sizeof title);
    if (strcmp(cls, "#32770") == 0 && strcmp(title, "Error") == 0 && IsWindowVisible(h)) {
        HWND txt = GetDlgItem(h, 0xFFFF); char msg[256] = "";
        if (txt) GetWindowTextA(txt, msg, sizeof msg);
        if (strstr(msg, "system menu item")) { found = h; return FALSE; }
    }
    return TRUE;
}
int main(int argc, char **argv) {
    int wait = argc > 1 ? atoi(argv[1]) : 60, i;
    for (i = 0; i < wait * 4; i++) {
        found = NULL; EnumWindows(enum_cb, 0);
        if (found) { PostMessageA(found, WM_COMMAND, IDOK, 0); printf("okclick: dismissed %p\n", (void*)found); Sleep(500); return 0; }
        Sleep(250);
    }
    printf("okclick: no dialog within %d s\n", wait); return 1;
}
