/* iphlpapi proxy for Viz Engine under Wine: forwards everything to iphlpapi_wine.dll (a copy of Wine's
 * builtin) except GetAdaptersAddresses, which marks adapters without any unicast address as Down.
 * ACE::get_ip_interfaces dereferences FirstUnicastAddress of every "Up" adapter without a null check. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
typedef ULONG (WINAPI *GAA_t)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
static GAA_t real_gaa(void)
{
    static GAA_t fn; if (!fn) { HMODULE h = LoadLibraryA("iphlpapi_wine.dll"); if (h) fn = (GAA_t)GetProcAddress(h, "GetAdaptersAddresses"); }
    return fn;
}
__declspec(dllexport) ULONG WINAPI GetAdaptersAddresses(ULONG family, ULONG flags, PVOID reserved, PIP_ADAPTER_ADDRESSES addrs, PULONG size)
{
    GAA_t fn = real_gaa(); ULONG ret;
    if (!fn) return ERROR_PROC_NOT_FOUND;
    ret = fn(family, flags, reserved, addrs, size);
    if (ret == NO_ERROR && addrs) {
        IP_ADAPTER_ADDRESSES *a;
        for (a = addrs; a; a = a->Next)
            if (!a->FirstUnicastAddress) a->OperStatus = IfOperStatusDown;
    }
    return ret;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h; (void)r; (void)p; return TRUE; }
