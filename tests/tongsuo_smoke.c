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
    ASN1_INTEGER *serial = NULL, *unrevoked_serial = NULL;
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
    unrevoked_serial = ASN1_INTEGER_new();
    now = X509_gmtime_adj(NULL, 0);
    next = X509_gmtime_adj(NULL, 86400);
    if (ca == NULL || leaf == NULL || store == NULL || ctx == NULL ||
        crl == NULL || revoked == NULL || serial == NULL ||
        unrevoked_serial == NULL || now == NULL ||
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

    /* A leaf issued by this CA must not validate against an unrelated trust anchor. */
    {
        X509 *unrelated_ca = X509_new();
        X509_STORE *unrelated_store = X509_STORE_new();
        X509_STORE_CTX *unrelated_ctx = X509_STORE_CTX_new();
        if (unrelated_ca == NULL || unrelated_store == NULL || unrelated_ctx == NULL ||
            X509_set_version(unrelated_ca, 2) != 1 ||
            ASN1_INTEGER_set(X509_get_serialNumber(unrelated_ca), 200) != 1 ||
            X509_gmtime_adj(X509_getm_notBefore(unrelated_ca), 0) == NULL ||
            X509_gmtime_adj(X509_getm_notAfter(unrelated_ca), 86400) == NULL ||
            X509_set_pubkey(unrelated_ca, leaf_key) != 1 ||
            X509_NAME_add_entry_by_txt(X509_get_subject_name(unrelated_ca), "CN",
                MBSTRING_ASC, (const unsigned char *)"unrelated test CA", -1, -1, 0) != 1 ||
            X509_set_issuer_name(unrelated_ca, X509_get_subject_name(unrelated_ca)) != 1 ||
            X509_sign(unrelated_ca, leaf_key, EVP_sm3()) <= 0 ||
            X509_STORE_add_cert(unrelated_store, unrelated_ca) != 1 ||
            X509_STORE_CTX_init(unrelated_ctx, unrelated_store, leaf, NULL) != 1 ||
            X509_verify_cert(unrelated_ctx) == 1) {
            fprintf(stderr, "certificate chain unexpectedly validated against unrelated CA (error=%d)\\n",
                    unrelated_ctx != NULL ? X509_STORE_CTX_get_error(unrelated_ctx) : -1);
            X509_free(unrelated_ca);
            X509_STORE_CTX_free(unrelated_ctx);
            X509_STORE_free(unrelated_store);
            goto cleanup;
        }
        X509_free(unrelated_ca);
        X509_STORE_CTX_free(unrelated_ctx);
        X509_STORE_free(unrelated_store);
    }
    puts("X.509 chain rejects unrelated trust anchor: PASS");

    /* A CA:FALSE intermediate must not issue a valid chain. */
    {
        EVP_PKEY *nonca_key = TSAPI_SM2Keygen();
        X509 *nonca = X509_new();
        STACK_OF(X509) *untrusted = sk_X509_new_null();
        X509_STORE_CTX *nonca_ctx = X509_STORE_CTX_new();
        X509_EXTENSION *nonca_ext = NULL;
        int rejected = 0;
        if (nonca_key == NULL || nonca == NULL || untrusted == NULL || nonca_ctx == NULL ||
            X509_set_version(nonca, 2) != 1 ||
            ASN1_INTEGER_set(X509_get_serialNumber(nonca), 201) != 1 ||
            X509_gmtime_adj(X509_getm_notBefore(nonca), 0) == NULL ||
            X509_gmtime_adj(X509_getm_notAfter(nonca), 86400) == NULL ||
            X509_set_pubkey(nonca, nonca_key) != 1 ||
            (nonca_ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints, "critical,CA:FALSE")) == NULL ||
            X509_add_ext(nonca, nonca_ext, -1) != 1 ||
            (X509_EXTENSION_free(nonca_ext), nonca_ext = NULL, 0) ||
            X509_NAME_add_entry_by_txt(X509_get_subject_name(nonca), "CN", MBSTRING_ASC,
                (const unsigned char *)"non-CA intermediate", -1, -1, 0) != 1 ||
            X509_set_issuer_name(nonca, X509_get_subject_name(ca)) != 1 ||
            X509_sign(nonca, ca_key, EVP_sm3()) <= 0 ||
            X509_set_issuer_name(leaf, X509_get_subject_name(nonca)) != 1 ||
            X509_sign(leaf, nonca_key, EVP_sm3()) <= 0 ||
            sk_X509_push(untrusted, nonca) == 0 ||
            X509_STORE_CTX_init(nonca_ctx, store, leaf, untrusted) != 1) {
            fprintf(stderr, "failed to construct non-CA intermediate chain\n");
            X509_EXTENSION_free(nonca_ext); sk_X509_free(untrusted);
            X509_STORE_CTX_free(nonca_ctx); X509_free(nonca); EVP_PKEY_free(nonca_key);
            goto cleanup;
        }
        rejected = X509_verify_cert(nonca_ctx) != 1 &&
                   X509_STORE_CTX_get_error(nonca_ctx) == X509_V_ERR_INVALID_CA;
        X509_EXTENSION_free(nonca_ext); sk_X509_free(untrusted);
        X509_STORE_CTX_free(nonca_ctx); X509_free(nonca); EVP_PKEY_free(nonca_key);
        if (!rejected || X509_set_issuer_name(leaf, X509_get_subject_name(ca)) != 1 ||
            X509_sign(leaf, ca_key, EVP_sm3()) <= 0) {
            fprintf(stderr, "X.509 chain did not reject CA:FALSE intermediate\n");
            goto cleanup;
        }
    }
    puts("X.509 chain rejects CA:FALSE intermediate: PASS");

    /* A matching signature alone is insufficient when the issuer name is wrong. */
    {
        X509_NAME *wrong_issuer = X509_NAME_new();
        if (wrong_issuer == NULL ||
            X509_NAME_add_entry_by_txt(wrong_issuer, "CN", MBSTRING_ASC,
                (const unsigned char *)"wrong issuer name", -1, -1, 0) != 1 ||
            X509_set_issuer_name(leaf, wrong_issuer) != 1 ||
            X509_sign(leaf, ca_key, EVP_sm3()) <= 0 ||
            X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
            X509_verify_cert(ctx) == 1 ||
            X509_STORE_CTX_get_error(ctx) != X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
            fprintf(stderr, "X.509 chain did not reject wrong issuer name (error=%d)\\n",
                    X509_STORE_CTX_get_error(ctx));
            X509_NAME_free(wrong_issuer);
            goto cleanup;
        }
        X509_NAME_free(wrong_issuer);
        if (X509_set_issuer_name(leaf, X509_get_subject_name(ca)) != 1 ||
            X509_sign(leaf, ca_key, EVP_sm3()) <= 0) {
            fprintf(stderr, "failed to restore leaf issuer\\n");
            goto cleanup;
        }
    }
    puts("X.509 chain rejects wrong issuer name: PASS");

    /* Reject a leaf certificate whose validity starts in the future. */
    if (X509_gmtime_adj(X509_getm_notBefore(leaf), 3600) == NULL ||
        X509_sign(leaf, ca_key, EVP_sm3()) <= 0 ||
        X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
        X509_verify_cert(ctx) == 1 ||
        X509_STORE_CTX_get_error(ctx) != X509_V_ERR_CERT_NOT_YET_VALID) {
        fprintf(stderr, "X.509 chain did not reject not-yet-valid leaf (error=%d)\\n",
                X509_STORE_CTX_get_error(ctx));
        goto cleanup;
    }
    if (X509_gmtime_adj(X509_getm_notBefore(leaf), -3660) == NULL ||
        X509_sign(leaf, ca_key, EVP_sm3()) <= 0) {
        fprintf(stderr, "failed to restore leaf notBefore\\n");
        goto cleanup;
    }
    puts("X.509 chain rejects not-yet-valid leaf: PASS");

    /* Reject an expired leaf certificate. */
    if (X509_gmtime_adj(X509_getm_notAfter(leaf), -3600) == NULL ||
        X509_sign(leaf, ca_key, EVP_sm3()) <= 0 ||
        X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
        X509_verify_cert(ctx) == 1 ||
        X509_STORE_CTX_get_error(ctx) != X509_V_ERR_CERT_HAS_EXPIRED) {
        fprintf(stderr, "X.509 chain did not reject expired leaf (error=%d)\\n",
                X509_STORE_CTX_get_error(ctx));
        goto cleanup;
    }
    if (X509_gmtime_adj(X509_getm_notAfter(leaf), 86400) == NULL ||
        X509_sign(leaf, ca_key, EVP_sm3()) <= 0) {
        fprintf(stderr, "failed to restore leaf notAfter\\n");
        goto cleanup;
    }
    puts("X.509 chain rejects expired leaf: PASS");

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
    found = NULL;
    if (ASN1_INTEGER_set(unrevoked_serial, 102) != 1 ||
        X509_CRL_get0_by_serial(crl, &found, unrevoked_serial) != 0 ||
        found != NULL) {
        fprintf(stderr, "CRL incorrectly reports an unrevoked serial\n");
        goto cleanup;
    }
    if (X509_STORE_add_crl(store, crl) != 1 ||
        X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK) != 1 ||
        X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
        X509_verify_cert(ctx) == 1 ||
        X509_STORE_CTX_get_error(ctx) != X509_V_ERR_CERT_REVOKED) {
        fprintf(stderr, "X509_STORE CRL_CHECK did not reject revoked leaf (error=%d)\\n",
                X509_STORE_CTX_get_error(ctx));
        goto cleanup;
    }

    /* Reissue the same leaf identity with an unlisted serial and verify it. */
    if (ASN1_INTEGER_set(X509_get_serialNumber(leaf), 102) != 1 ||
        X509_sign(leaf, ca_key, EVP_sm3()) <= 0 ||
        X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
        X509_verify_cert(ctx) != 1) {
        fprintf(stderr, "X509_STORE CRL_CHECK rejected an unrevoked leaf (error=%d)\\n",
                X509_STORE_CTX_get_error(ctx));
        goto cleanup;
    }
    /* An expired CRL must fail closed rather than validate the leaf. */
    {
        ASN1_TIME *expired = X509_gmtime_adj(NULL, -86400);
        if (expired == NULL || X509_CRL_set1_nextUpdate(crl, expired) != 1 ||
            X509_CRL_sign(crl, ca_key, EVP_sm3()) <= 0 ||
            X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
            X509_verify_cert(ctx) == 1 ||
            X509_STORE_CTX_get_error(ctx) != X509_V_ERR_CRL_HAS_EXPIRED) {
            fprintf(stderr, "X509_STORE CRL_CHECK did not reject expired CRL (error=%d)\\n",
                    X509_STORE_CTX_get_error(ctx));
            ASN1_TIME_free(expired);
            goto cleanup;
        }
        ASN1_TIME_free(expired);
    }
    puts("X509_STORE CRL_CHECK expired CRL rejection: PASS");

    /* A CRL whose lastUpdate is in the future must also be rejected. */
    {
        ASN1_TIME *future = X509_gmtime_adj(NULL, 3600);
        ASN1_TIME *future_next = X509_gmtime_adj(NULL, 90000);
        if (future == NULL || future_next == NULL ||
            X509_CRL_set1_lastUpdate(crl, future) != 1 ||
            X509_CRL_set1_nextUpdate(crl, future_next) != 1 ||
            X509_CRL_sign(crl, ca_key, EVP_sm3()) <= 0 ||
            X509_STORE_CTX_init(ctx, store, leaf, NULL) != 1 ||
            X509_verify_cert(ctx) == 1 ||
            X509_STORE_CTX_get_error(ctx) != X509_V_ERR_CRL_NOT_YET_VALID) {
            fprintf(stderr, "X509_STORE CRL_CHECK did not reject future CRL (error=%d)\\n",
                    X509_STORE_CTX_get_error(ctx));
            ASN1_TIME_free(future);
            ASN1_TIME_free(future_next);
            goto cleanup;
        }
        ASN1_TIME_free(future);
        ASN1_TIME_free(future_next);
    }
    puts("X509_STORE CRL_CHECK future CRL rejection: PASS");
    /* CRL_CHECK must fail closed when the store has no CRL. */
    {
        X509_STORE *empty_store = X509_STORE_new();
        X509_STORE_CTX *empty_ctx = X509_STORE_CTX_new();
        if (empty_store == NULL || empty_ctx == NULL ||
            X509_STORE_add_cert(empty_store, ca) != 1 ||
            X509_STORE_set_flags(empty_store, X509_V_FLAG_CRL_CHECK) != 1 ||
            X509_STORE_CTX_init(empty_ctx, empty_store, leaf, NULL) != 1 ||
            X509_verify_cert(empty_ctx) == 1 ||
            X509_STORE_CTX_get_error(empty_ctx) != X509_V_ERR_UNABLE_TO_GET_CRL) {
            fprintf(stderr, "X509_STORE CRL_CHECK did not fail without CRL (error=%d)\\n",
                    empty_ctx != NULL ? X509_STORE_CTX_get_error(empty_ctx) : -1);
            X509_STORE_CTX_free(empty_ctx);
            X509_STORE_free(empty_store);
            goto cleanup;
        }
        X509_STORE_CTX_free(empty_ctx);
        X509_STORE_free(empty_store);
    }
    puts("X509_STORE CRL_CHECK missing CRL rejection: PASS");




    /* A matching CRL issuer name with a signature from the wrong key must fail. */
    {
        X509_STORE *bad_store = X509_STORE_new();
        X509_STORE_CTX *bad_ctx = X509_STORE_CTX_new();
        ASN1_TIME *valid_from = X509_gmtime_adj(NULL, 0);
        ASN1_TIME *valid_until = X509_gmtime_adj(NULL, 86400);
        if (bad_store == NULL || bad_ctx == NULL || valid_from == NULL ||
            valid_until == NULL ||
            X509_CRL_set1_lastUpdate(crl, valid_from) != 1 ||
            X509_CRL_set1_nextUpdate(crl, valid_until) != 1 ||
            X509_CRL_sign(crl, leaf_key, EVP_sm3()) <= 0 ||
            X509_STORE_add_cert(bad_store, ca) != 1 ||
            X509_STORE_add_crl(bad_store, crl) != 1 ||
            X509_STORE_set_flags(bad_store, X509_V_FLAG_CRL_CHECK) != 1 ||
            X509_STORE_CTX_init(bad_ctx, bad_store, leaf, NULL) != 1 ||
            X509_verify_cert(bad_ctx) == 1 ||
            X509_STORE_CTX_get_error(bad_ctx) != X509_V_ERR_CRL_SIGNATURE_FAILURE) {
            fprintf(stderr, "X509_STORE CRL_CHECK did not reject invalid CRL signature (error=%d)\\n",
                    bad_ctx != NULL ? X509_STORE_CTX_get_error(bad_ctx) : -1);
            ASN1_TIME_free(valid_from);
            ASN1_TIME_free(valid_until);
            X509_STORE_CTX_free(bad_ctx);
            X509_STORE_free(bad_store);
            goto cleanup;
        }
        ASN1_TIME_free(valid_from);
        ASN1_TIME_free(valid_until);
        X509_STORE_CTX_free(bad_ctx);
        X509_STORE_free(bad_store);
    }
    puts("X509_STORE CRL_CHECK invalid CRL signature rejection: PASS");

    /* A CRL naming a different issuer must not satisfy this CA's CRL check. */
    {
        X509_STORE *issuer_store = X509_STORE_new();
        X509_STORE_CTX *issuer_ctx = X509_STORE_CTX_new();
        X509_NAME *other_issuer = X509_NAME_new();
        if (issuer_store == NULL || issuer_ctx == NULL || other_issuer == NULL ||
            X509_NAME_add_entry_by_txt(other_issuer, "CN", MBSTRING_ASC,
                (const unsigned char *)"unrelated CRL issuer", -1, -1, 0) != 1 ||
            X509_CRL_set_issuer_name(crl, other_issuer) != 1 ||
            X509_CRL_sign(crl, ca_key, EVP_sm3()) <= 0 ||
            X509_STORE_add_cert(issuer_store, ca) != 1 ||
            X509_STORE_add_crl(issuer_store, crl) != 1 ||
            X509_STORE_set_flags(issuer_store, X509_V_FLAG_CRL_CHECK) != 1 ||
            X509_STORE_CTX_init(issuer_ctx, issuer_store, leaf, NULL) != 1 ||
            X509_verify_cert(issuer_ctx) == 1 ||
            X509_STORE_CTX_get_error(issuer_ctx) != X509_V_ERR_UNABLE_TO_GET_CRL) {
            fprintf(stderr, "X509_STORE CRL_CHECK did not reject mismatched CRL issuer (error=%d)\\n",
                    issuer_ctx != NULL ? X509_STORE_CTX_get_error(issuer_ctx) : -1);
            X509_NAME_free(other_issuer);
            X509_STORE_CTX_free(issuer_ctx);
            X509_STORE_free(issuer_store);
            goto cleanup;
        }
        X509_NAME_free(other_issuer);
        X509_STORE_CTX_free(issuer_ctx);
        X509_STORE_free(issuer_store);
    }
    puts("X509_STORE CRL_CHECK mismatched CRL issuer rejection: PASS");

    puts("X509_STORE CRL_CHECK revoked rejection and unrevoked acceptance: PASS");

    puts("CA-to-leaf chain and CRL revoked/unrevoked serial lookup: PASS");
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
    ASN1_INTEGER_free(unrevoked_serial);
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
