#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/tsapi.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <stdio.h>
#include <string.h>

static int certificate_roundtrip(EVP_PKEY *key, EVP_PKEY *wrong_key)
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

    if (X509_verify(cert, key) != 1 || X509_verify(cert, wrong_key) == 1) {
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

static int crl_roundtrip(EVP_PKEY *key, EVP_PKEY *wrong_key)
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
        X509_CRL_set_issuer_name(crl, X509_get_subject_name(cert)) != 1) {
        fprintf(stderr, "CRL creation/signing failed\n");
        goto cleanup;
    }
    last_update = X509_gmtime_adj(NULL, 0);
    next_update = X509_gmtime_adj(NULL, 86400);
    if (last_update == NULL || next_update == NULL ||
        X509_CRL_set1_lastUpdate(crl, last_update) != 1 ||
        X509_CRL_set1_nextUpdate(crl, next_update) != 1 ||
        X509_CRL_sign(crl, key, EVP_sm3()) <= 0) {
        fprintf(stderr, "CRL validity or signing failed\n");
        goto cleanup;
    }

    bio = BIO_new(BIO_s_mem());
    if (bio == NULL || PEM_write_bio_X509_CRL(bio, crl) != 1) {
        fprintf(stderr, "PEM CRL encoding failed\n");
        goto cleanup;
    }
    decoded = PEM_read_bio_X509_CRL(bio, NULL, NULL, NULL);
    if (decoded == NULL || X509_CRL_verify(decoded, key) != 1 ||
        X509_CRL_verify(decoded, wrong_key) == 1) {
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
        X509_CRL_verify(decoded, key) != 1 ||
        X509_CRL_verify(decoded, wrong_key) == 1) {
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


static int ca_chain_and_revocation(EVP_PKEY *ca_key, EVP_PKEY *leaf_key)
{
    X509 *ca = NULL, *leaf = NULL;
    X509_STORE *store = NULL;
    X509_STORE_CTX *ctx = NULL;
    X509_CRL *crl = NULL;
    X509_REVOKED *revoked = NULL, *found = NULL;
    ASN1_INTEGER *serial = NULL;
    ASN1_TIME *now = NULL, *next = NULL;
    int ok = 0;
    X509_EXTENSION *ext = NULL;

    ca = X509_new();
    leaf = X509_new();
    store = X509_STORE_new();
    ctx = X509_STORE_CTX_new();
    crl = X509_CRL_new();
    revoked = X509_REVOKED_new();
    serial = ASN1_INTEGER_new();
    now = X509_gmtime_adj(NULL, 0);
    next = X509_gmtime_adj(NULL, 86400);
    if (ca == NULL || leaf == NULL || store == NULL || ctx == NULL ||
        crl == NULL || revoked == NULL || serial == NULL || now == NULL ||
        next == NULL ||
        X509_set_version(ca, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(ca), 100) != 1 ||
        X509_gmtime_adj(X509_getm_notBefore(ca), 0) == NULL ||
        X509_gmtime_adj(X509_getm_notAfter(ca), 86400) == NULL ||
        X509_set_pubkey(ca, ca_key) != 1 ||
        (ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints, "critical,CA:TRUE")) == NULL ||
        X509_add_ext(ca, ext, -1) != 1 ||
        (X509_EXTENSION_free(ext), ext = NULL, 0) ||
        (ext = X509V3_EXT_conf_nid(NULL, NULL, NID_key_usage, "critical,keyCertSign,cRLSign")) == NULL ||
        X509_add_ext(ca, ext, -1) != 1 ||
        (X509_EXTENSION_free(ext), ext = NULL, 0) ||
        X509_NAME_add_entry_by_txt(X509_get_subject_name(ca), "CN", MBSTRING_ASC,
            (const unsigned char *)"certservers test CA", -1, -1, 0) != 1 ||
        X509_set_issuer_name(ca, X509_get_subject_name(ca)) != 1 ||
        X509_sign(ca, ca_key, EVP_sm3()) <= 0 ||
        X509_set_version(leaf, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(leaf), 101) != 1 ||
        X509_gmtime_adj(X509_getm_notBefore(leaf), 0) == NULL ||
        X509_gmtime_adj(X509_getm_notAfter(leaf), 86400) == NULL ||
        X509_set_pubkey(leaf, leaf_key) != 1 ||
        (ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints, "critical,CA:FALSE")) == NULL ||
        X509_add_ext(leaf, ext, -1) != 1 ||
        (X509_EXTENSION_free(ext), ext = NULL, 0) ||
        (ext = X509V3_EXT_conf_nid(NULL, NULL, NID_key_usage, "critical,digitalSignature")) == NULL ||
        X509_add_ext(leaf, ext, -1) != 1 ||
        (X509_EXTENSION_free(ext), ext = NULL, 0) ||
        X509_NAME_add_entry_by_txt(X509_get_subject_name(leaf), "CN", MBSTRING_ASC,
            (const unsigned char *)"certservers test leaf", -1, -1, 0) != 1 ||
        X509_set_issuer_name(leaf, X509_get_subject_name(ca)) != 1 ||
        X509_sign(leaf, ca_key, EVP_sm3()) <= 0 ||
        X509_STORE_add_cert(store, ca) != 1 ||
        X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
        X509_verify_cert(ctx) != 1) {
        fprintf(stderr, "CA-to-leaf chain verification failed\n");
        goto cleanup;
    }

    if (X509_CRL_set_version(crl, 1) != 1 ||
        X509_CRL_set_issuer_name(crl, X509_get_subject_name(ca)) != 1 ||
        X509_CRL_set1_lastUpdate(crl, now) != 1 ||
        X509_CRL_set1_nextUpdate(crl, next) != 1 ||
        ASN1_INTEGER_set(serial, 101) != 1 ||
        X509_REVOKED_set_serialNumber(revoked, serial) != 1 ||
        X509_REVOKED_set_revocationDate(revoked, now) != 1 ||
        X509_CRL_add0_revoked(crl, revoked) != 1) {
        fprintf(stderr, "CRL revoked-entry construction failed\n");
        goto cleanup;
    }
    revoked = NULL;
    if (X509_CRL_sign(crl, ca_key, EVP_sm3()) <= 0 ||
        X509_CRL_verify(crl, ca_key) != 1 ||
        X509_CRL_get0_by_serial(crl, &found, X509_get_serialNumber(leaf)) != 1 ||
        found == NULL ||
        ASN1_INTEGER_cmp(X509_REVOKED_get0_serialNumber(found),
                        X509_get_serialNumber(leaf)) != 0) {
        fprintf(stderr, "CRL signature or revoked serial lookup failed\n");
        goto cleanup;
    }
    puts("CA-to-leaf chain and verified CRL revoked serial lookup: PASS");
    ok = 1;
cleanup:
    X509_EXTENSION_free(ext);
    X509_free(ca);
    X509_free(leaf);
    X509_STORE_free(store);
    X509_STORE_CTX_free(ctx);
    X509_CRL_free(crl);
    X509_REVOKED_free(revoked);
    ASN1_INTEGER_free(serial);
    ASN1_TIME_free(now);
    ASN1_TIME_free(next);
    return ok;
}

int main(void)
{
    static const unsigned char message[] = "certservers Tongsuo integration smoke test";
    size_t siglen = 0;
    unsigned char *sig = NULL;
    char *version = NULL;
    EVP_PKEY *key = NULL, *wrong_key = NULL, *leaf_key = NULL;
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

    wrong_key = TSAPI_SM2Keygen();
    if (wrong_key == NULL) {
        fprintf(stderr, "second TSAPI_SM2Keygen failed\n");
        goto cleanup;
    }

    sig = TSAPI_SM2Sign(key, message, sizeof(message) - 1, &siglen);
    if (sig == NULL || siglen == 0 ||
        TSAPI_SM2Verify(key, message, sizeof(message) - 1, sig, siglen) != 1 ||
        TSAPI_SM2Verify(wrong_key, message, sizeof(message) - 1, sig, siglen) == 1 ||
        TSAPI_SM2Verify(key, (const unsigned char *)"altered", 7, sig, siglen) == 1) {
        fprintf(stderr, "TSAPI SM2 sign/verify failed\n");
        goto cleanup;
    }
    puts("TSAPI SM2 keygen/sign/verify: PASS");

    leaf_key = TSAPI_SM2Keygen();
    if (leaf_key == NULL || !certificate_roundtrip(key, wrong_key) ||
        !crl_roundtrip(key, wrong_key) || !ca_chain_and_revocation(key, leaf_key))
        goto cleanup;

    ok = 1;
cleanup:
    OPENSSL_free(sig);
    OPENSSL_free(version);
    EVP_PKEY_free(leaf_key);
    EVP_PKEY_free(wrong_key);
    EVP_PKEY_free(key);
    return ok ? 0 : 1;
}
