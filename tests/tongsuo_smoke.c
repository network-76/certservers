#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/tsapi.h>
#include <openssl/x509.h>

#include <stdio.h>
#include <string.h>

static int certificate_roundtrip(EVP_PKEY *key)
{
    X509 *cert = NULL, *decoded = NULL;
    BIO *bio = NULL;
    unsigned char *der = NULL, *cursor;
    int der_len, ok = 0;

    cert = X509_new();
    if (cert == NULL ||
        X509_set_version(cert, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) != 1 ||
        X509_gmtime_adj(X509_getm_notBefore(cert), 0) == NULL ||
        X509_gmtime_adj(X509_getm_notAfter(cert), 86400) == NULL ||
        X509_set_pubkey(cert, key) != 1 ||
        X509_set_issuer_name(cert, X509_get_subject_name(cert)) != 1 ||
        X509_NAME_add_entry_by_txt(X509_get_subject_name(cert), "CN",
            MBSTRING_ASC, (const unsigned char *)"certservers smoke", -1, -1, 0) != 1 ||
        X509_set_issuer_name(cert, X509_get_subject_name(cert)) != 1 ||
        X509_sign(cert, key, EVP_sm3()) <= 0) {
        fprintf(stderr, "self-signed certificate creation/signing failed\n");
        goto cleanup;
    }

    if (X509_verify(cert, key) != 1) {
        fprintf(stderr, "certificate signature verification failed\n");
        goto cleanup;
    }

    bio = BIO_new(BIO_s_mem());
    if (bio == NULL || PEM_write_bio_X509(bio, cert) != 1) {
        fprintf(stderr, "PEM certificate encoding failed\n");
        goto cleanup;
    }
    decoded = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    if (decoded == NULL || X509_verify(decoded, key) != 1) {
        fprintf(stderr, "PEM certificate roundtrip failed\n");
        goto cleanup;
    }
    X509_free(decoded);
    decoded = NULL;
    BIO_free(bio);
    bio = NULL;

    der_len = i2d_X509(cert, &der);
    if (der_len <= 0 || der == NULL) {
        fprintf(stderr, "DER certificate encoding failed\n");
        goto cleanup;
    }
    cursor = der;
    decoded = d2i_X509(NULL, (const unsigned char **)&cursor, der_len);
    if (decoded == NULL || cursor != der + der_len ||
        X509_verify(decoded, key) != 1) {
        fprintf(stderr, "DER certificate roundtrip failed\n");
        goto cleanup;
    }

    puts("X.509 self-signed certificate, signature, PEM and DER: PASS");
    ok = 1;
cleanup:
    OPENSSL_free(der);
    BIO_free(bio);
    X509_free(decoded);
    X509_free(cert);
    return ok;
}

static int crl_roundtrip(EVP_PKEY *key)
{
    X509 *cert = NULL;
    X509_CRL *crl = NULL, *decoded = NULL;
    BIO *bio = NULL;
    ASN1_TIME *last_update = NULL, *next_update = NULL;
    unsigned char *der = NULL, *cursor;
    int der_len, ok = 0;

    cert = X509_new();
    crl = X509_CRL_new();
    if (cert == NULL || crl == NULL ||
        X509_set_subject_name(cert, X509_get_subject_name(cert)) != 1 ||
        X509_NAME_add_entry_by_txt(X509_get_subject_name(cert), "CN",
            MBSTRING_ASC, (const unsigned char *)"certservers CRL issuer", -1, -1, 0) != 1 ||
        X509_CRL_set_version(crl, 1) != 1 ||
        X509_CRL_set_issuer_name(crl, X509_get_subject_name(cert)) != 1 ||
        X509_CRL_set1_lastUpdate(crl, X509_gmtime_adj(NULL, 0)) != 1 ||
        X509_CRL_set1_nextUpdate(crl, X509_gmtime_adj(NULL, 86400)) != 1 ||
        X509_CRL_sign(crl, key, EVP_sm3()) <= 0) {
        fprintf(stderr, "CRL creation/signing failed\n");
        goto cleanup;
    }
    last_update = X509_gmtime_adj(NULL, 0);
    next_update = X509_gmtime_adj(NULL, 86400);
    if (last_update == NULL || next_update == NULL ||
        X509_CRL_set1_lastUpdate(crl, last_update) != 1 ||
        X509_CRL_set1_nextUpdate(crl, next_update) != 1 ||
        X509_CRL_sign(crl, key, EVP_sm3()) <= 0) {
        fprintf(stderr, "CRL validity or signing failed\\n");
        goto cleanup;
    }

    bio = BIO_new(BIO_s_mem());
    if (bio == NULL || PEM_write_bio_X509_CRL(bio, crl) != 1) {
        fprintf(stderr, "PEM CRL encoding failed\n");
        goto cleanup;
    }
    decoded = PEM_read_bio_X509_CRL(bio, NULL, NULL, NULL);
    if (decoded == NULL || X509_CRL_verify(decoded, key) != 1) {
        fprintf(stderr, "PEM CRL roundtrip/signature verification failed\n");
        goto cleanup;
    }
    X509_CRL_free(decoded);
    decoded = NULL;
    BIO_free(bio);
    bio = NULL;

    der_len = i2d_X509_CRL(crl, &der);
    if (der_len <= 0 || der == NULL) {
        fprintf(stderr, "DER CRL encoding failed\n");
        goto cleanup;
    }
    cursor = der;
    decoded = d2i_X509_CRL(NULL, (const unsigned char **)&cursor, der_len);
    if (decoded == NULL || cursor != der + der_len ||
        X509_CRL_verify(decoded, key) != 1) {
        fprintf(stderr, "DER CRL roundtrip/signature verification failed\n");
        goto cleanup;
    }

    puts("X.509 CRL, signature, PEM and DER: PASS");
    ok = 1;
cleanup:
    OPENSSL_free(der);
    BIO_free(bio);
    X509_CRL_free(decoded);
    X509_CRL_free(crl);
    ASN1_TIME_free(last_update);
    ASN1_TIME_free(next_update);
    X509_free(cert);
    return ok;
}

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
    if (sig == NULL || siglen == 0 ||
        TSAPI_SM2Verify(key, message, sizeof(message) - 1, sig, siglen) != 1) {
        fprintf(stderr, "TSAPI SM2 sign/verify failed\n");
        goto cleanup;
    }
    puts("TSAPI SM2 keygen/sign/verify: PASS");

    if (!certificate_roundtrip(key) || !crl_roundtrip(key))
        goto cleanup;

    ok = 1;
cleanup:
    OPENSSL_free(sig);
    OPENSSL_free(version);
    EVP_PKEY_free(key);
    return ok ? 0 : 1;
}
