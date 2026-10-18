/*
 * opengl32 proxy for the Viz Engine under Wine: "swap copy" emulation.
 *
 * Problem: the engine's classic (V3) scene editor calls SwapBuffers on its window more often than it
 * draws into it (about two swaps per vblank, one of them without new drawing). On Windows windowed
 * OpenGL a swap is a copy and the back buffer keeps its contents, so a swap without drawing shows the
 * same picture again. On Linux/NVIDIA (GLX, Xwayland) a swap exchanges buffers, so every second
 * present shows the never-drawn buffer: the view flickers or stays black.
 *
 * Fix: forward every export to opengl32_wine.dll (a copy of Wine's builtin opengl32.dll) except
 * wglSwapBuffers / wglSwapLayerBuffers / wglDeleteContext. Around each swap of a double-buffered
 * window the back buffer is saved into a per-context renderbuffer and blitted back afterwards, so the
 * back buffer after the swap holds what was just presented (WGL_SWAP_COPY_ARB behaviour).
 *
 * Env: VIZ_GL_SWAPCOPY=0 disables the emulation; VIZ_GL_SWAPCOPY_LOG=1 logs to stderr;
 *      VIZ_GL_SWAPCOPY_LOG=2 additionally logs, for the first 400 swaps, how many clears / draw calls /
 *      blits the swapping thread issued since its previous swap (diagnoses "swap without drawing").
 * Build: x86_64-w64-mingw32-gcc -O2 -Wall -shared -Wl,--kill-at -static-libgcc -o opengl32.dll proxy.c opengl32.def -lgdi32 -luser32
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef unsigned char GLboolean;

#define GL_NO_ERROR                  0
#define GL_BACK                      0x0405
#define GL_DRAW_BUFFER               0x0C01
#define GL_READ_BUFFER               0x0C02
#define GL_SCISSOR_TEST              0x0C11
#define GL_DOUBLEBUFFER              0x0C32
#define GL_COLOR_BUFFER_BIT          0x4000
#define GL_NEAREST                   0x2600
#define GL_RGBA8                     0x8058
#define GL_SAMPLE_BUFFERS            0x80A8
#define GL_SAMPLES                   0x80A9
#define GL_RASTERIZER_DISCARD        0x8C89
#define GL_DRAW_FRAMEBUFFER_BINDING  0x8CA6
#define GL_RENDERBUFFER_BINDING      0x8CA7
#define GL_READ_FRAMEBUFFER          0x8CA8
#define GL_DRAW_FRAMEBUFFER          0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING  0x8CAA
#define GL_FRAMEBUFFER_COMPLETE      0x8CD5
#define GL_COLOR_ATTACHMENT0         0x8CE0
#define GL_RENDERBUFFER              0x8D41
#define GL_FRAMEBUFFER_SRGB          0x8DB9

typedef BOOL (WINAPI *PFN_SWAP)(HDC);
typedef BOOL (WINAPI *PFN_DELCTX)(HGLRC);
typedef HGLRC (WINAPI *PFN_GETCTX)(void);
typedef HDC (WINAPI *PFN_GETDC)(void);
typedef PROC (WINAPI *PFN_GETPROC)(LPCSTR);
typedef void (WINAPI *PFN_GETINTV)(GLenum, GLint *);
typedef GLboolean (WINAPI *PFN_ISENABLED)(GLenum);
typedef void (WINAPI *PFN_ENDIS)(GLenum);
typedef void (WINAPI *PFN_BUF)(GLenum);
typedef GLenum (WINAPI *PFN_GETERROR)(void);
typedef void (WINAPI *PFN_GEN)(GLsizei, GLuint *);
typedef void (WINAPI *PFN_BIND)(GLenum, GLuint);
typedef GLboolean (WINAPI *PFN_IS)(GLuint);
typedef void (WINAPI *PFN_RBSTORAGEMS)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
typedef void (WINAPI *PFN_FBRB)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum (WINAPI *PFN_CHECK)(GLenum);
typedef void (WINAPI *PFN_BLIT)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

static HMODULE real;
static PFN_SWAP p_swap;
static PFN_DELCTX p_delctx;
static PFN_GETCTX p_getctx;
static PFN_GETDC p_getdc;
static PFN_GETPROC p_getproc;
static PFN_GETINTV p_getintv;
static PFN_ISENABLED p_isenabled;
static PFN_ENDIS p_enable, p_disable;
static PFN_BUF p_readbuf, p_drawbuf;
static PFN_GETERROR p_geterror;
static int g_enabled = 1, g_log;
static CRITICAL_SECTION g_cs;

/* --- diagnostics: per-thread counters of work issued between swaps --- */
typedef struct { unsigned long clears, draws, blits; } Counters;
static DWORD g_tls = TLS_OUT_OF_INDEXES;
static Counters *tc(void)
{
    static Counters dummy;
    if (g_tls == TLS_OUT_OF_INDEXES) return &dummy;
    Counters *c = TlsGetValue(g_tls);
    if (!c && (c = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *c))) TlsSetValue(g_tls, c);
    return c ? c : &dummy;
}
#define t_clears (tc()->clears)
#define t_draws (tc()->draws)
#define t_blits (tc()->blits)
static long g_diag_swaps;
typedef void (WINAPI *PFN_V1)(GLenum);
typedef void (WINAPI *PFN_CLEAR)(GLbitfield);
typedef void (WINAPI *PFN_DA)(GLenum, GLint, GLsizei);
typedef void (WINAPI *PFN_DE)(GLenum, GLsizei, GLenum, const void *);
typedef void (WINAPI *PFN_CLS)(GLsizei, GLenum, const void *);
static PFN_CLEAR r_glClear; static PFN_DA r_glDrawArrays; static PFN_DE r_glDrawElements;
static PFN_V1 r_glBegin, r_glCallList; static PFN_CLS r_glCallLists;
static PFN_BLIT r_glBlitFramebuffer;
typedef void (WINAPI *PFN_ANY6)(void *, void *, void *, void *, void *, void *, void *, void *); /* up to 8 integer/pointer args */
#define N_GPA 8
static const char *gpa_names[N_GPA] = { "glDrawRangeElements", "glDrawElementsBaseVertex", "glDrawElementsInstanced",
    "glDrawArraysInstanced", "glDrawRangeElementsBaseVertex", "glDrawElementsInstancedBaseVertex", "glMultiDrawElements",
    "glMultiDrawArrays" };
static PFN_ANY6 gpa_real[N_GPA];

typedef struct {
    HGLRC ctx;
    int failed;           /* emulation impossible for this context (missing entry points, incomplete FBO) */
    GLuint fbo, rb;
    GLint w, h, samples;
    PFN_GEN gen_fb, gen_rb;
    PFN_BIND bind_fb, bind_rb;
    PFN_IS is_fb;
    PFN_RBSTORAGEMS rb_storage_ms;
    PFN_FBRB fb_rb;
    PFN_CHECK check;
    PFN_BLIT blit;
    unsigned long swaps;
} CtxState;

#define MAX_CTX 64
static CtxState g_ctx[MAX_CTX];

static void logf_(const char *fmt, ...);
#define DRAW_WRAP(i) static void WINAPI gpa_wrap##i(void *a, void *b, void *c, void *d, void *e, void *f, void *g, void *h) { t_draws++; gpa_real[i](a, b, c, d, e, f, g, h); }
DRAW_WRAP(0) DRAW_WRAP(1) DRAW_WRAP(2) DRAW_WRAP(3) DRAW_WRAP(4) DRAW_WRAP(5) DRAW_WRAP(6) DRAW_WRAP(7)
static PFN_ANY6 gpa_wraps[N_GPA] = { gpa_wrap0, gpa_wrap1, gpa_wrap2, gpa_wrap3, gpa_wrap4, gpa_wrap5, gpa_wrap6, gpa_wrap7 };
static void WINAPI blit_wrap(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield m, GLenum fl)
{ t_blits++; r_glBlitFramebuffer(a, b, c, d, e, f, g, h, m, fl); }

static void logf_(const char *fmt, ...)
{
    if (!g_log) return;
    va_list ap; va_start(ap, fmt);
    fputs("glswapcopy: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); fflush(stderr);
    va_end(ap);
}

static BOOL load_real(void)
{
    if (real) return TRUE;
    if (!(real = LoadLibraryA("opengl32_wine.dll"))) return FALSE;
    p_swap = (PFN_SWAP)GetProcAddress(real, "wglSwapBuffers");
    p_delctx = (PFN_DELCTX)GetProcAddress(real, "wglDeleteContext");
    p_getctx = (PFN_GETCTX)GetProcAddress(real, "wglGetCurrentContext");
    p_getdc = (PFN_GETDC)GetProcAddress(real, "wglGetCurrentDC");
    p_getproc = (PFN_GETPROC)GetProcAddress(real, "wglGetProcAddress");
    p_getintv = (PFN_GETINTV)GetProcAddress(real, "glGetIntegerv");
    p_isenabled = (PFN_ISENABLED)GetProcAddress(real, "glIsEnabled");
    p_enable = (PFN_ENDIS)GetProcAddress(real, "glEnable");
    p_disable = (PFN_ENDIS)GetProcAddress(real, "glDisable");
    p_readbuf = (PFN_BUF)GetProcAddress(real, "glReadBuffer");
    p_drawbuf = (PFN_BUF)GetProcAddress(real, "glDrawBuffer");
    p_geterror = (PFN_GETERROR)GetProcAddress(real, "glGetError");
    r_glClear = (PFN_CLEAR)GetProcAddress(real, "glClear");
    r_glDrawArrays = (PFN_DA)GetProcAddress(real, "glDrawArrays");
    r_glDrawElements = (PFN_DE)GetProcAddress(real, "glDrawElements");
    r_glBegin = (PFN_V1)GetProcAddress(real, "glBegin");
    r_glCallList = (PFN_V1)GetProcAddress(real, "glCallList");
    r_glCallLists = (PFN_CLS)GetProcAddress(real, "glCallLists");
    return p_swap != NULL;
}

static CtxState *get_state(HGLRC ctx)
{
    CtxState *free_slot = NULL;
    for (int i = 0; i < MAX_CTX; i++) {
        if (g_ctx[i].ctx == ctx) return &g_ctx[i];
        if (!g_ctx[i].ctx && !free_slot) free_slot = &g_ctx[i];
    }
    if (!free_slot) return NULL;
    memset(free_slot, 0, sizeof *free_slot);
    free_slot->ctx = ctx;
    CtxState *s = free_slot;
    s->gen_fb = (PFN_GEN)p_getproc("glGenFramebuffers");
    s->gen_rb = (PFN_GEN)p_getproc("glGenRenderbuffers");
    s->bind_fb = (PFN_BIND)p_getproc("glBindFramebuffer");
    s->bind_rb = (PFN_BIND)p_getproc("glBindRenderbuffer");
    s->is_fb = (PFN_IS)p_getproc("glIsFramebuffer");
    s->rb_storage_ms = (PFN_RBSTORAGEMS)p_getproc("glRenderbufferStorageMultisample");
    s->fb_rb = (PFN_FBRB)p_getproc("glFramebufferRenderbuffer");
    s->check = (PFN_CHECK)p_getproc("glCheckFramebufferStatus");
    s->blit = (PFN_BLIT)p_getproc("glBlitFramebuffer");
    if (!s->gen_fb || !s->gen_rb || !s->bind_fb || !s->bind_rb || !s->is_fb || !s->rb_storage_ms || !s->fb_rb ||
        !s->check || !s->blit) {
        logf_("context %p: missing FBO entry points, emulation off", ctx);
        s->failed = 1;
    }
    return s;
}

/* Saved GL state around our blits. */
typedef struct { GLint draw_fb, read_fb, rb; GLboolean scissor, srgb, discard; } Saved;

static void save_state(CtxState *s, Saved *v)
{
    p_getintv(GL_DRAW_FRAMEBUFFER_BINDING, &v->draw_fb);
    p_getintv(GL_READ_FRAMEBUFFER_BINDING, &v->read_fb);
    p_getintv(GL_RENDERBUFFER_BINDING, &v->rb);
    v->scissor = p_isenabled(GL_SCISSOR_TEST);
    v->srgb = p_isenabled(GL_FRAMEBUFFER_SRGB);
    v->discard = p_isenabled(GL_RASTERIZER_DISCARD);
    if (v->scissor) p_disable(GL_SCISSOR_TEST);
    if (v->srgb) p_disable(GL_FRAMEBUFFER_SRGB);
    if (v->discard) p_disable(GL_RASTERIZER_DISCARD);
}

static void restore_state(CtxState *s, Saved *v)
{
    s->bind_fb(GL_DRAW_FRAMEBUFFER, v->draw_fb);
    s->bind_fb(GL_READ_FRAMEBUFFER, v->read_fb);
    s->bind_rb(GL_RENDERBUFFER, v->rb);
    if (v->scissor) p_enable(GL_SCISSOR_TEST);
    if (v->srgb) p_enable(GL_FRAMEBUFFER_SRGB);
    if (v->discard) p_enable(GL_RASTERIZER_DISCARD);
}

/* Make sure s->fbo has a colour renderbuffer matching the default framebuffer (size + samples). */
static BOOL ensure_fbo(CtxState *s, GLint w, GLint h)
{
    GLint samples = 0, sb = 0;
    s->bind_fb(GL_DRAW_FRAMEBUFFER, 0);
    p_getintv(GL_SAMPLE_BUFFERS, &sb);
    if (sb) p_getintv(GL_SAMPLES, &samples);
    if (s->fbo && !s->is_fb(s->fbo)) s->fbo = s->rb = 0; /* context handle reused */
    if (s->fbo && s->w == w && s->h == h && s->samples == samples) return TRUE;
    if (!s->fbo) { s->gen_fb(1, &s->fbo); s->gen_rb(1, &s->rb); }
    s->bind_rb(GL_RENDERBUFFER, s->rb);
    s->rb_storage_ms(GL_RENDERBUFFER, samples, GL_RGBA8, w, h);
    s->bind_fb(GL_DRAW_FRAMEBUFFER, s->fbo);
    s->fb_rb(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, s->rb);
    GLenum st = s->check(GL_DRAW_FRAMEBUFFER);
    s->w = w; s->h = h; s->samples = samples;
    logf_("context %p: save buffer %dx%d samples %d status 0x%x", s->ctx, w, h, samples, st);
    if (st != GL_FRAMEBUFFER_COMPLETE) { s->failed = 1; return FALSE; }
    return TRUE;
}

__declspec(dllexport) BOOL WINAPI wglSwapBuffers(HDC hdc)
{
    if (!load_real()) return FALSE;
    HGLRC ctx = g_enabled ? p_getctx() : NULL;
    if (g_log >= 2 && InterlockedIncrement(&g_diag_swaps) <= 400) {
        GLint dfb = -1;
        if (p_getctx()) p_getintv(GL_DRAW_FRAMEBUFFER_BINDING, &dfb);
        logf_("diag: tid %lu hdc %p hwnd %p ctx %p curdc %p drawfb %d | since last swap: clears %lu draws %lu blits %lu",
              GetCurrentThreadId(), hdc, WindowFromDC(hdc), p_getctx(), p_getdc(), dfb, t_clears, t_draws, t_blits);
    }
    { Counters *c = tc(); c->clears = c->draws = c->blits = 0; }
    if (!ctx) return p_swap(hdc);
    HDC cur = p_getdc();
    if (cur != hdc && (!cur || WindowFromDC(cur) != WindowFromDC(hdc))) {
        static LONG warned;
        if (InterlockedIncrement(&warned) <= 10)
            logf_("swap of hdc %p (hwnd %p) while ctx %p is current on hdc %p (hwnd %p): not emulated", hdc,
                  WindowFromDC(hdc), ctx, cur, cur ? WindowFromDC(cur) : NULL);
        return p_swap(hdc);
    }

    EnterCriticalSection(&g_cs);
    CtxState *s = get_state(ctx);
    LeaveCriticalSection(&g_cs);
    if (!s || s->failed) return p_swap(hdc);

    GLint dbl = 0;
    p_getintv(GL_DOUBLEBUFFER, &dbl);
    RECT rc;
    HWND hwnd = WindowFromDC(hdc);
    if (!dbl || !hwnd || !GetClientRect(hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return p_swap(hdc);
    GLint w = rc.right, h = rc.bottom;

    Saved v;
    save_state(s, &v);
    BOOL ok = ensure_fbo(s, w, h);
    if (ok) {
        /* back buffer -> save buffer */
        GLint rbuf = GL_BACK;
        s->bind_fb(GL_READ_FRAMEBUFFER, 0);
        p_getintv(GL_READ_BUFFER, &rbuf);
        p_readbuf(GL_BACK);
        s->bind_fb(GL_DRAW_FRAMEBUFFER, s->fbo);
        s->blit(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        p_readbuf(rbuf);
    }
    restore_state(s, &v);

    BOOL ret = p_swap(hdc);

    if (ok) {
        /* save buffer -> new back buffer, so the back buffer again holds the presented picture */
        save_state(s, &v);
        GLint dbuf = GL_BACK;
        s->bind_fb(GL_DRAW_FRAMEBUFFER, 0);
        p_getintv(GL_DRAW_BUFFER, &dbuf);
        p_drawbuf(GL_BACK);
        s->bind_fb(GL_READ_FRAMEBUFFER, s->fbo);
        s->blit(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        p_drawbuf(dbuf);
        restore_state(s, &v);
        if (g_log && (s->swaps < 5 || s->swaps % 1000 == 0))
            logf_("context %p hdc %p swap %lu (%dx%d) glGetError=0x%x", ctx, hdc, s->swaps, w, h, p_geterror());
    }
    s->swaps++;
    return ret;
}

__declspec(dllexport) BOOL WINAPI wglSwapLayerBuffers(HDC hdc, UINT planes)
{
    if (planes & WGL_SWAP_MAIN_PLANE) return wglSwapBuffers(hdc);
    return TRUE;
}

__declspec(dllexport) BOOL WINAPI wglDeleteContext(HGLRC ctx)
{
    if (!load_real()) return FALSE;
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_CTX; i++)
        if (g_ctx[i].ctx == ctx) g_ctx[i].ctx = NULL; /* GL objects die with the context */
    LeaveCriticalSection(&g_cs);
    return p_delctx(ctx);
}

/* --- diagnostic pass-through wrappers (always installed, only count) --- */
__declspec(dllexport) void WINAPI glClear(GLbitfield m) { t_clears++; load_real(); r_glClear(m); }
__declspec(dllexport) void WINAPI glDrawArrays(GLenum a, GLint b, GLsizei c) { t_draws++; load_real(); r_glDrawArrays(a, b, c); }
__declspec(dllexport) void WINAPI glDrawElements(GLenum a, GLsizei b, GLenum c, const void *d) { t_draws++; load_real(); r_glDrawElements(a, b, c, d); }
__declspec(dllexport) void WINAPI glBegin(GLenum a) { t_draws++; load_real(); r_glBegin(a); }
__declspec(dllexport) void WINAPI glCallList(GLuint a) { t_draws++; load_real(); r_glCallList(a); }
__declspec(dllexport) void WINAPI glCallLists(GLsizei a, GLenum b, const void *c) { t_draws++; load_real(); r_glCallLists(a, b, c); }

__declspec(dllexport) PROC WINAPI wglGetProcAddress(LPCSTR name)
{
    if (!load_real()) return NULL;
    PROC p = p_getproc(name);
    if (!p || g_log < 2 || ((ULONG_PTR)name >> 16) == 0) return p;
    if (!strcmp(name, "glBlitFramebuffer") || !strcmp(name, "glBlitFramebufferEXT")) {
        r_glBlitFramebuffer = (PFN_BLIT)p; return (PROC)blit_wrap;
    }
    for (int i = 0; i < N_GPA; i++)
        if (!strcmp(name, gpa_names[i])) { gpa_real[i] = (PFN_ANY6)p; return (PROC)gpa_wraps[i]; }
    return p;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID p)
{
    (void)h; (void)p;
    if (reason == DLL_PROCESS_ATTACH) {
        char buf[16];
        InitializeCriticalSection(&g_cs);
        g_tls = TlsAlloc();
        if (GetEnvironmentVariableA("VIZ_GL_SWAPCOPY", buf, sizeof buf) && buf[0] == '0') g_enabled = 0;
        if (GetEnvironmentVariableA("VIZ_GL_SWAPCOPY_LOG", buf, sizeof buf) && buf[0] >= '1') g_log = buf[0] - '0';
    }
    return TRUE;
}
