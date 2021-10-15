{
  'variables': {
    'openssl_sources': [
      'openssl/crypto/bn/bn_asm.c',
      'openssl/crypto/des/des_enc.c',
      'openssl/crypto/des/fcrypt_b.c',
      'openssl/crypto/md5/md5_dgst.c',
      'openssl/crypto/md5/md5_one.c',
      'openssl/crypto/md5/md5_sha1.c',
      'openssl/providers/implementations/ciphers/cipher_blowfish.c',
      'openssl/providers/implementations/ciphers/cipher_blowfish_hw.c',
      'openssl/providers/implementations/ciphers/cipher_cast5.c',
      'openssl/providers/implementations/ciphers/cipher_cast5_hw.c',
      'openssl/providers/implementations/ciphers/cipher_des.c',
      'openssl/providers/implementations/ciphers/cipher_des_hw.c',
      'openssl/providers/implementations/ciphers/cipher_desx.c',
      'openssl/providers/implementations/ciphers/cipher_desx_hw.c',
      'openssl/providers/implementations/ciphers/cipher_idea.c',
      'openssl/providers/implementations/ciphers/cipher_idea_hw.c',
      'openssl/providers/implementations/ciphers/cipher_rc2.c',
      'openssl/providers/implementations/ciphers/cipher_rc2_hw.c',
      'openssl/providers/implementations/ciphers/cipher_rc4.c',
      'openssl/providers/implementations/ciphers/cipher_rc4_hmac_md5.c',
      'openssl/providers/implementations/ciphers/cipher_rc4_hmac_md5_hw.c',
      'openssl/providers/implementations/ciphers/cipher_rc4_hw.c',
      'openssl/providers/implementations/ciphers/cipher_seed.c',
      'openssl/providers/implementations/ciphers/cipher_seed_hw.c',
      'openssl/providers/implementations/ciphers/cipher_tdes_common.c',
      'openssl/providers/implementations/digests/md4_prov.c',
      'openssl/providers/implementations/digests/mdc2_prov.c',
      'openssl/providers/implementations/digests/ripemd_prov.c',
      'openssl/providers/implementations/digests/wp_prov.c',
      'openssl/providers/implementations/kdfs/pbkdf1.c',
      'openssl/providers/prov_running.c',
      'openssl/providers/legacyprov.c',

    ],
    'openssl_sources_aix64-gcc-as': [

    ],
    'openssl_defines_aix64-gcc-as': [
      'NDEBUG',
      'B_ENDIAN',
      'OPENSSL_BUILDING_OPENSSL',
    ],
    'openssl_cflags_aix64-gcc-as': [
      '-O',
      '-maix64 -pthread',
      '-O',
    ],
    'openssl_ex_libs_aix64-gcc-as': [
      '-pthread',
    ],
    'version_script': ''
  },
  'include_dirs': [
    '.',
    './include',
    './crypto',
    './crypto/include/internal',
    './providers/common/include',
  ],
  'defines': ['<@(openssl_defines_aix64-gcc-as)'],
  'cflags': ['<@(openssl_cflags_aix64-gcc-as)'],
  'libraries': ['<@(openssl_ex_libs_aix64-gcc-as)'],

  'sources': ['<@(openssl_sources)', '<@(openssl_sources_aix64-gcc-as)'],
  'direct_dependent_settings': {
    'include_dirs': ['./include', '.'],
    'defines': ['<@(openssl_defines_aix64-gcc-as)'],
  },
}
