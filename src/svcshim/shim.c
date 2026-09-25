/* Fake SCM dispatcher so Viz GH Terminal can run as a plain console process under Wine
 * (the real Windows-service path never gets its .NET web server up under Wine's services.exe). */
#include <windows.h>
#include <stdio.h>
static LPHANDLER_FUNCTION g_handler; static DWORD g_magic = 0x5EC7;
static const SERVICE_TABLE_ENTRYA *g_entry;
static BOOL WINAPI ctrl_handler(DWORD type) { (void)type; if (g_handler) g_handler(SERVICE_CONTROL_STOP); return TRUE; }
static DWORD WINAPI svc_thread(LPVOID p) { SERVICE_TABLE_ENTRYA *e = p; char *argv[2] = { e->lpServiceName, NULL }; e->lpServiceProc(1, argv); return 0; }
__declspec(dllexport) BOOL WINAPI StartServiceCtrlDispatcherA(const SERVICE_TABLE_ENTRYA *table)
{
    HANDLE th[8]; int n = 0;
    fprintf(stderr, "[svcshim] running service '%s' as console process\n", table[0].lpServiceName);
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
    for (const SERVICE_TABLE_ENTRYA *e = table; e->lpServiceName && n < 8; e++, n++) th[n] = CreateThread(NULL, 0, svc_thread, (LPVOID)e, 0, NULL);
    WaitForMultipleObjects(n, th, TRUE, INFINITE);
    fprintf(stderr, "[svcshim] service main returned, exiting\n");
    return TRUE;
}
__declspec(dllexport) SERVICE_STATUS_HANDLE WINAPI RegisterServiceCtrlHandlerA(LPCSTR name, LPHANDLER_FUNCTION handler)
{ (void)name; g_handler = handler; return (SERVICE_STATUS_HANDLE)&g_magic; }
typedef BOOL (WINAPI *SSS_t)(SERVICE_STATUS_HANDLE, LPSERVICE_STATUS);
__declspec(dllexport) BOOL WINAPI SetServiceStatus(SERVICE_STATUS_HANDLE h, LPSERVICE_STATUS st)
{
    if (h == (SERVICE_STATUS_HANDLE)&g_magic) { fprintf(stderr, "[svcshim] SetServiceStatus state=%lu\n", st ? st->dwCurrentState : 0); return TRUE; }
    SSS_t real = (SSS_t)GetProcAddress(LoadLibraryA("advapi32.dll"), "SetServiceStatus"); return real ? real(h, st) : FALSE;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h; (void)r; (void)p; return TRUE; }
