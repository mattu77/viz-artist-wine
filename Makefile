# Builds every helper DLL/EXE with the mingw-w64 cross compiler (Fedora: mingw64-gcc; Debian: gcc-mingw-w64-x86-64).
CC      = x86_64-w64-mingw32-gcc
CFLAGS  = -O2 -Wall
DLLFLAGS = -shared -Wl,--kill-at -static-libgcc
OUT     = build

DLLS = $(OUT)/nvcuda.dll $(OUT)/vizasn1.dll $(OUT)/iphlpapi.dll $(OUT)/httpapi.dll $(OUT)/ADVAPI3Z.dll $(OUT)/SHEL32Z.dll
EXES = $(OUT)/CmWebAdmin.exe $(OUT)/okclick.exe $(OUT)/okqt.exe $(OUT)/dwtest.exe $(OUT)/adapters.exe

all: $(DLLS) $(EXES)

$(OUT):
	mkdir -p $(OUT)

$(OUT)/nvcuda.dll: src/nvcuda-stub/nvcuda.c | $(OUT)
	$(CC) $(CFLAGS) $(DLLFLAGS) -o $@ $<

$(OUT)/vizasn1.dll: src/vizasn1/vizasn1.c | $(OUT)
	$(CC) $(CFLAGS) $(DLLFLAGS) -o $@ $<

# forwards to iphlpapi_wine.dll = a copy of Wine's builtin iphlpapi.dll (see README)
$(OUT)/iphlpapi.dll: src/iphlpapi-proxy/proxy.c src/iphlpapi-proxy/iphlpapi.def | $(OUT)
	$(CC) $(CFLAGS) $(DLLFLAGS) -o $@ $^

# forwards to httpapi_wine.dll = a copy of Wine's builtin httpapi.dll (optional, see README)
$(OUT)/httpapi.dll: src/httpapi-proxy/proxy.c src/httpapi-proxy/httpapi.def | $(OUT)
	$(CC) $(CFLAGS) $(DLLFLAGS) -o $@ $^

$(OUT)/ADVAPI3Z.dll: src/svcshim/shim.c src/svcshim/shim.def | $(OUT)
	$(CC) $(CFLAGS) $(DLLFLAGS) -o $@ $^

$(OUT)/SHEL32Z.dll: src/shel32z/shel32z.c src/shel32z/shel32z.def | $(OUT)
	$(CC) $(CFLAGS) $(DLLFLAGS) -o $@ $^ -luser32

$(OUT)/CmWebAdmin.exe: src/stubsvc/stubsvc.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< -ladvapi32

$(OUT)/okclick.exe: src/okclick/okclick.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< -luser32

$(OUT)/okqt.exe: src/okqt/okqt.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< -luser32

$(OUT)/dwtest.exe: src/diag/dwtest.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< -ldwrite -lole32 -luuid

$(OUT)/adapters.exe: src/diag/adapters.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< -liphlpapi -lws2_32

clean:
	rm -rf $(OUT)

.PHONY: all clean
