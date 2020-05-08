#ifndef VAL_SECALGO_H_VALIDATOR
#define VAL_SECALGO_H_VALIDATOR

#define sldns_buffer                    gldns_buffer

#define nsec3_hash_algo_size_supported  _getdns_nsec3_hash_algo_size_supported
#define secalgo_nsec3_hash              _getdns_secalgo_nsec3_hash
#define secalgo_hash_sha256             _getdns_secalgo_hash_sha256
#define ds_digest_size_supported        _getdns_ds_digest_size_supported
#define secalgo_ds_digest               _getdns_secalgo_ds_digest
#define dnskey_algo_id_is_supported     _getdns_dnskey_algo_id_is_supported
#define verify_canonrrset               _getdns_verify_canonrrset
#define sec_status                      _getdns_sec_status
#define sec_status_secure               _getdns_sec_status_secure
#define sec_status_insecure             _getdns_sec_status_insecure
#define sec_status_unchecked            _getdns_sec_status_unchecked
#define sec_status_bogus                _getdns_sec_status_bogus
#define fake_sha1                       _getdns_fake_sha1
#define fake_dsa                        _getdns_fake_dsa


#define NSEC3_HASH_SHA1                 0x01

#define LDNS_SHA1                       GLDNS_SHA1
#define LDNS_SHA256                     GLDNS_SHA256
#define LDNS_SHA384                     GLDNS_SHA384
#define LDNS_HASH_GOST                  GLDNS_HASH_GOST
#define LDNS_RSAMD5                     GLDNS_RSAMD5
#define LDNS_DSA                        GLDNS_DSA
#define LDNS_DSA_NSEC3                  GLDNS_DSA_NSEC3
#define LDNS_RSASHA1                    GLDNS_RSASHA1
#define LDNS_RSASHA1_NSEC3              GLDNS_RSASHA1_NSEC3
#define LDNS_RSASHA256                  GLDNS_RSASHA256
#define LDNS_RSASHA512                  GLDNS_RSASHA512
#define LDNS_ECDSAP256SHA256            GLDNS_ECDSAP256SHA256
#define LDNS_ECDSAP384SHA384            GLDNS_ECDSAP384SHA384
#define LDNS_ECC_GOST                   GLDNS_ECC_GOST
#define LDNS_ED25519                    GLDNS_ED25519
#define LDNS_ED448                      GLDNS_ED448
#define sldns_ed255192pkey_raw          gldns_ed255192pkey_raw
#define sldns_ed4482pkey_raw            gldns_ed4482pkey_raw
#define sldns_key_EVP_load_gost_id      gldns_key_EVP_load_gost_id
#define sldns_digest_evp                gldns_digest_evp
#define sldns_key_buf2dsa_raw           gldns_key_buf2dsa_raw
#define sldns_key_buf2rsa_raw           gldns_key_buf2rsa_raw
#define sldns_gost2pkey_raw             gldns_gost2pkey_raw
#define sldns_ecdsa2pkey_raw            gldns_ecdsa2pkey_raw
#define sldns_buffer_begin              gldns_buffer_begin
#define sldns_buffer_limit              gldns_buffer_limit

#include "util/val_secalgo.h"

#endif
