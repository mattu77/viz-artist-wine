/* httpapi proxy for Viz GH Terminal under Wine: Wine's HttpInitialize calls StartService("HTTP"), which deadlocks
 * against services.exe's startup lock while the Terminal service itself is still starting. The HTTP driver is
 * started beforehand by the launcher, so HttpInitialize just succeeds here; everything else forwards to Wine's builtin. */
#include <windows.h>
typedef struct { USHORT HttpApiMajorVersion, HttpApiMinorVersion; } HTTPAPI_VER;
__declspec(dllexport) ULONG WINAPI HttpInitialize(HTTPAPI_VER version, ULONG flags, void *reserved)
{ (void)version; (void)flags; (void)reserved; return NO_ERROR; }
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h; (void)r; (void)p; return TRUE; }
