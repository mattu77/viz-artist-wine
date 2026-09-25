/* Minimal Win32 service: reports RUNNING and idles. Used to replace CmWebAdmin.exe under Wine. */
#include <windows.h>
static SERVICE_STATUS_HANDLE h; static SERVICE_STATUS st; static HANDLE stop_evt;
static void WINAPI ctrl(DWORD c){ if(c==SERVICE_CONTROL_STOP||c==SERVICE_CONTROL_SHUTDOWN){ st.dwCurrentState=SERVICE_STOP_PENDING; SetServiceStatus(h,&st); SetEvent(stop_evt);} }
static void WINAPI svcmain(DWORD argc, LPWSTR *argv){ (void)argc;(void)argv;
  h=RegisterServiceCtrlHandlerW(L"CmWebAdmin.exe",ctrl); if(!h) return;
  st.dwServiceType=SERVICE_WIN32_OWN_PROCESS; st.dwControlsAccepted=SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN;
  st.dwCurrentState=SERVICE_RUNNING; SetServiceStatus(h,&st);
  stop_evt=CreateEventW(NULL,TRUE,FALSE,NULL); WaitForSingleObject(stop_evt,INFINITE);
  st.dwCurrentState=SERVICE_STOPPED; SetServiceStatus(h,&st); }
int main(void){ SERVICE_TABLE_ENTRYW t[]={{(LPWSTR)L"CmWebAdmin.exe",svcmain},{NULL,NULL}}; StartServiceCtrlDispatcherW(t); return 0; }
