/* Minimal nvcuda.dll stub for Viz Engine under Wine: reports "no CUDA device". */
#include <windows.h>
typedef int CUresult;
#define CUDA_SUCCESS 0
#define CUDA_ERROR_NOT_INITIALIZED 3
#define CUDA_ERROR_NO_DEVICE 100
#define STUB(name) __declspec(dllexport) CUresult name(void) { return CUDA_ERROR_NOT_INITIALIZED; }
__declspec(dllexport) CUresult cuInit(unsigned int flags) { (void)flags; return CUDA_ERROR_NO_DEVICE; }
__declspec(dllexport) CUresult cuDeviceGetCount(int *count) { if (count) *count = 0; return CUDA_ERROR_NOT_INITIALIZED; }
__declspec(dllexport) CUresult cuGetErrorString(CUresult err, const char **str) {
    static const char *msgs[] = { "no error", "invalid value", "out of memory", "initialization error" };
    if (str) *str = (err == CUDA_ERROR_NO_DEVICE) ? "no CUDA-capable device is detected (wine stub)" :
                    (err >= 0 && err < 4) ? msgs[err] : "unknown error (wine stub)";
    return CUDA_SUCCESS;
}
STUB(cuDeviceGetName) STUB(cuCtxCreate_v2) STUB(cuDeviceGet) STUB(cuCtxDestroy_v2)
STUB(cuCtxPushCurrent_v2) STUB(cuMemFree_v2) STUB(cuCtxPopCurrent_v2) STUB(cuMemAllocPitch_v2)
STUB(cuGraphicsUnmapResources) STUB(cuGraphicsGLRegisterImage) STUB(cuGraphicsSubResourceGetMappedArray)
STUB(cuGraphicsMapResources) STUB(cuGraphicsUnregisterResource) STUB(cuMemcpy2D_v2)
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h;(void)r;(void)p; return TRUE; }
