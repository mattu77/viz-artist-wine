/* shell32 proxy for Qt's qwindows.dll under Wine: Wine's SHGetStockIconInfo returns S_OK with a NULL icon for
 * SIID_WARNING/INFO/ERROR/SHIELD, which makes Qt call GetIconInfo(NULL) and Viz Artist pop a critical dialog per icon.
 * Here those stock icons come from user32's standard icons instead. */
#include <windows.h>
#include <shellapi.h>
typedef HRESULT (WINAPI *SGSII_t)(SHSTOCKICONID, UINT, SHSTOCKICONINFO *);
__declspec(dllexport) HRESULT WINAPI SHGetStockIconInfo(SHSTOCKICONID siid, UINT flags, SHSTOCKICONINFO *sii)
{
    SGSII_t real = (SGSII_t)GetProcAddress(LoadLibraryA("shell32.dll"), "SHGetStockIconInfo");
    HRESULT hr = real ? real(siid, flags, sii) : E_FAIL;
    if (!(flags & SHGSI_ICON) || !sii) return hr;
    if (SUCCEEDED(hr) && sii->hIcon) return hr;
    LPCWSTR res = (LPCWSTR)IDI_APPLICATION;
    switch (siid) { case SIID_WARNING: res = (LPCWSTR)IDI_WARNING; break; case SIID_INFO: res = (LPCWSTR)IDI_INFORMATION; break;
                    case SIID_ERROR: res = (LPCWSTR)IDI_ERROR; break; case SIID_SHIELD: res = (LPCWSTR)IDI_SHIELD; break;
                    case SIID_HELP: res = (LPCWSTR)IDI_QUESTION; break; default: break; }
    int cx = (flags & SHGSI_SMALLICON) ? GetSystemMetrics(SM_CXSMICON) : GetSystemMetrics(SM_CXICON);
    sii->hIcon = (HICON)LoadImageW(NULL, res, IMAGE_ICON, cx, cx, LR_SHARED);
    if (!sii->hIcon) sii->hIcon = LoadIconW(NULL, res);
    return sii->hIcon ? S_OK : hr;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h; (void)r; (void)p; return TRUE; }
