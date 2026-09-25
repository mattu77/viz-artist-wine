#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>
int main(int argc, char **argv) {
    IDWriteFactory *f = NULL; IDWriteFontCollection *c = NULL; UINT32 idx = 0, n; BOOL exists = FALSE;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown**)&f);
    printf("factory hr=%08lx\n", hr);
    hr = IDWriteFactory_GetSystemFontCollection(f, &c, TRUE); n = IDWriteFontCollection_GetFontFamilyCount(c);
    printf("system collection hr=%08lx families=%u\n", hr, n);
    const WCHAR *names[] = { L"Tahoma", L"Arial", L"Verdana", L"Segoe UI", L"MS Shell Dlg", L"MS Sans Serif" };
    for (int i = 0; i < 6; i++) { hr = IDWriteFontCollection_FindFamilyName(c, names[i], &idx, &exists); printf("  %-14ls exists=%d idx=%u hr=%08lx\n", names[i], exists, idx, hr); }
    return 0;
}
