#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
int main(void){
    ULONG sz=0; GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &sz);
    IP_ADAPTER_ADDRESSES *a = (IP_ADAPTER_ADDRESSES*)malloc(sz);
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, a, &sz) != NO_ERROR) { printf("failed\n"); return 1; }
    for (; a; a = a->Next) {
        int n = 0; for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u; u = u->Next) n++;
        printf("idx=%lu name=%s type=%lu oper=%d unicast=%d friendly=%ls\n", a->IfIndex, a->AdapterName, a->IfType, (int)a->OperStatus, n, a->FriendlyName);
    }
    return 0;
}
