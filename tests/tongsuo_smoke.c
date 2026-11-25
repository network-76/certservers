#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/tsapi.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned char message[] = "certservers Tongsuo integration smoke test";
    size_t siglen = 0;
    unsigned char *sig = NULL;
    char *version = NULL;
    EVP_PKEY *key = NULL;
    int ok = 0;

    version = TSAPI_Version();
    if (version == NULL || strstr(version, "8.5.0") == NULL) {
        fprintf(stderr, "unexpected Tongsuo TSAPI version: %s\n",
                version != NULL ? version : "(null)");
        goto cleanup;
    }
    printf("Tongsuo TSAPI version: %s\n", version);

    key = TSAPI_SM2Keygen();
    if (key == NULL) {
        fprintf(stderr, "TSAPI_SM2Keygen failed\n");
        goto cleanup;
    }

    sig = TSAPI_SM2Sign(key, message, sizeof(message) - 1, &siglen);
    if (sig == NULL || siglen == 0) {
        fprintf(stderr, "TSAPI_SM2Sign failed\n");
        goto cleanup;
    }

    if (TSAPI_SM2Verify(key, message, sizeof(message) - 1, sig, siglen) != 1) {
        fprintf(stderr, "TSAPI_SM2Verify failed\n");
        goto cleanup;
    }

    puts("TSAPI SM2 keygen/sign/verify: PASS");
    ok = 1;

cleanup:
    OPENSSL_free(sig);
    OPENSSL_free(version);
    EVP_PKEY_free(key);
    return ok ? 0 : 1;
}
