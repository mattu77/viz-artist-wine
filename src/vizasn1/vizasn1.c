/* Replacement for Wine's stub WVTAsn1SpcStatementTypeDecode (wintrust).
 * Decodes SpcStatementType ::= SEQUENCE OF OBJECT IDENTIFIER into SPC_STATEMENT_TYPE
 * using the classic CryptDllDecodeObject (7-argument) calling convention. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

typedef struct { DWORD cKeyPurposeId; LPSTR *rgpszKeyPurposeId; } STMT_TYPE;

static BOOL der_len(const BYTE *p, DWORD avail, DWORD *len, DWORD *hdr)
{
    if (avail < 2) return FALSE;
    if (p[1] < 0x80) { *len = p[1]; *hdr = 2; }
    else { DWORD n = p[1] & 0x7f, i; if (n == 0 || n > 4 || avail < 2 + n) return FALSE;
           *len = 0; for (i = 0; i < n; i++) *len = (*len << 8) | p[2 + i]; *hdr = 2 + n; }
    return *hdr + *len <= avail;
}

static DWORD oid_to_str(const BYTE *b, DWORD n, char *out, DWORD outsz)
{
    char tmp[256]; DWORD pos = 0, i; unsigned long long v = 0; int first = 1;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        v = (v << 7) | (b[i] & 0x7f);
        if (!(b[i] & 0x80)) {
            if (first) { pos += snprintf(tmp + pos, sizeof(tmp) - pos, "%llu.%llu", v < 80 ? v / 40 : 2ULL, v < 80 ? v % 40 : v - 80); first = 0; }
            else pos += snprintf(tmp + pos, sizeof(tmp) - pos, ".%llu", v);
            v = 0;
            if (pos >= sizeof(tmp) - 1) return 0;
        }
    }
    if (first) return 0;
    if (out) { if (outsz < pos + 1) return 0; memcpy(out, tmp, pos + 1); }
    return pos + 1;
}

__declspec(dllexport) BOOL WINAPI WVTAsn1SpcStatementTypeDecode(DWORD dwCertEncodingType, LPCSTR lpszStructType,
        const BYTE *pbEncoded, DWORD cbEncoded, DWORD dwFlags, void *pvStructInfo, DWORD *pcbStructInfo)
{
    DWORD seqlen, seqhdr, off, count = 0, strbytes = 0, need, i;
    (void)dwCertEncodingType; (void)lpszStructType; (void)dwFlags;
    if (!pbEncoded || cbEncoded < 2 || pbEncoded[0] != 0x30 || !der_len(pbEncoded, cbEncoded, &seqlen, &seqhdr)) {
        SetLastError(CRYPT_E_ASN1_BADTAG); return FALSE; }
    /* pass 1: validate and size */
    for (off = seqhdr; off < seqhdr + seqlen; ) {
        DWORD l, h, s;
        if (pbEncoded[off] != 0x06 || !der_len(pbEncoded + off, seqhdr + seqlen - off, &l, &h)) { SetLastError(CRYPT_E_ASN1_BADTAG); return FALSE; }
        s = oid_to_str(pbEncoded + off + h, l, NULL, 0);
        if (!s) { SetLastError(CRYPT_E_ASN1_CORRUPT); return FALSE; }
        count++; strbytes += s; off += h + l;
    }
    need = sizeof(STMT_TYPE) + count * sizeof(LPSTR) + strbytes;
    if (!pvStructInfo) { *pcbStructInfo = need; return TRUE; }
    if (*pcbStructInfo < need) { *pcbStructInfo = need; SetLastError(ERROR_MORE_DATA); return FALSE; }
    *pcbStructInfo = need;
    {
        STMT_TYPE *st = pvStructInfo;
        LPSTR *arr = (LPSTR *)(st + 1);
        char *str = (char *)(arr + count);
        st->cKeyPurposeId = count; st->rgpszKeyPurposeId = count ? arr : NULL;
        for (off = seqhdr, i = 0; i < count; i++) {
            DWORD l, h, s; der_len(pbEncoded + off, seqhdr + seqlen - off, &l, &h);
            s = oid_to_str(pbEncoded + off + h, l, str, strbytes);
            arr[i] = str; str += s; strbytes -= s; off += h + l;
        }
    }
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h; (void)r; (void)p; return TRUE; }
