GOST_KEY_TRANSPORT *make_rfc4490_keytransport_2001(EVP_PKEY *pubk,
                                                   BIGNUM *eph_key,
                                                   const unsigned char *key,
                                                   size_t keylen,
                                                   unsigned char *ukm,
                                                   size_t ukm_len);

int decrypt_rfc4490_shared_key_2001(EVP_PKEY *priv,
                                    GOST_KEY_TRANSPORT * gkt,
                                    unsigned char *key_buf, int key_buf_len);
