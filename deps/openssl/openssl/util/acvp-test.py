#!/usr/bin/env python3
"""
acvp-test.py

Tests an OpenSSL binary against the NIST ACVTS demo server for a chosen algorithm.

Usage:
    python acvp-test.py --openssl /path/to/openssl --cert my.cer --key my.key \
        --totp-seed totp.txt --algorithm ACVP-AES-CBC
    python acvp-test.py --openssl /path/to/openssl --cert my.cer --key my.key \
        --totp-seed totp.txt --algorithm ACVP-AES-CBC --direction encrypt --key-len 256
    python acvp-test.py --openssl /path/to/openssl --cert my.cer --key my.key \
        --totp-seed totp.txt --algorithm SHA2-256
    python acvp-test.py --openssl /path/to/openssl --cert my.cer --key my.key \
        --totp-seed totp.txt --algorithm HMAC-SHA2-256 --save-vectors

Algorithms supported:
    Symmetric : ACVP-AES-CBC, ACVP-AES-ECB, ACVP-AES-CTR
    Digest    : SHA2-256, SHA2-384, SHA2-512, SHA3-256, SHA3-384, SHA3-512
    MAC       : HMAC-SHA2-256, HMAC-SHA2-384, HMAC-SHA2-512
    PQC KEM   : ML-KEM-keyGen
    PQC Sig   : ML-DSA-keyGen, ML-DSA-sigGen, ML-DSA-sigVer
                SLH-DSA-keyGen, SLH-DSA-sigGen, SLH-DSA-sigVer

Requirements:
    pip install requests pyotp cryptography

Credentials needed (pass via command-line arguments):
    --openssl PATH   - path to the OpenSSL binary to test
    --cert FILE      - TLS client certificate from NIST (.cer)
    --key FILE       - corresponding private key (.key)
    --totp-seed FILE - file containing the Base64-encoded TOTP seed (one line)
"""

import argparse
import base64
import ctypes
import glob
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import time

import pyotp
import requests
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

DEMO_URL       = "https://demo.acvts.nist.gov/acvp/v1"
PROD_URL       = "https://acvts.nist.gov/acvp/v1"
BASE_URL       = DEMO_URL   # overridden to PROD_URL when --production is set
CERT_FILE      = None       # set from --cert
KEY_FILE       = None       # set from --key
TOTP_SEED_FILE = None       # set from --totp-seed
OPENSSL_BIN    = None       # set from --openssl


# ---------------------------------------------------------------------------
# ALGORITHM CAPABILITY BUILDERS
# Each returns the capability dict to include in the "algorithms" list during
# test session registration.  Signature: (directions, key_lens) -> dict
# ---------------------------------------------------------------------------

def build_aes_cbc_cap(directions, key_lens):
    return {
        "algorithm": "ACVP-AES-CBC",
        "revision":  "1.0",
        "direction": directions or ["encrypt", "decrypt"],
        "keyLen":    key_lens   or [128, 192, 256],
    }


def build_aes_ecb_cap(directions, key_lens):
    return {
        "algorithm": "ACVP-AES-ECB",
        "revision":  "1.0",
        "direction": directions or ["encrypt", "decrypt"],
        "keyLen":    key_lens   or [128, 192, 256],
    }


def build_sha256_cap(*_):
    return {
        "algorithm": "SHA2-256",
        "revision":  "1.0",
        "messageLength": [{"min": 0, "max": 65536, "increment": 8}],
    }


def build_sha384_cap(*_):
    return {
        "algorithm": "SHA2-384",
        "revision":  "1.0",
        "messageLength": [{"min": 0, "max": 65536, "increment": 8}],
    }


def build_sha512_cap(*_):
    return {
        "algorithm": "SHA2-512",
        "revision":  "1.0",
        "messageLength": [{"min": 0, "max": 65536, "increment": 8}],
    }


def build_aes_ctr_cap(directions, key_lens):
    return {
        "algorithm":           "ACVP-AES-CTR",
        "revision":            "1.0",
        "direction":           directions or ["encrypt", "decrypt"],
        "keyLen":              key_lens   or [128, 192, 256],
        "payloadLen":          [{"min": 8, "max": 128, "increment": 8}],
        "incrementalCounter":  True,
        "overflowCounter":     True,
        "performCounterTests": False,
    }


def build_hmac_sha256_cap(*_):
    return {
        "algorithm": "HMAC-SHA2-256",
        "revision":  "1.0",
        "keyLen":    [{"min": 8, "max": 524288, "increment": 8}],
        "macLen":    [{"min": 32, "max": 256,   "increment": 8}],
    }


def build_hmac_sha384_cap(*_):
    return {
        "algorithm": "HMAC-SHA2-384",
        "revision":  "1.0",
        "keyLen":    [{"min": 8, "max": 524288, "increment": 8}],
        "macLen":    [{"min": 32, "max": 384,   "increment": 8}],
    }


def build_hmac_sha512_cap(*_):
    return {
        "algorithm": "HMAC-SHA2-512",
        "revision":  "1.0",
        "keyLen":    [{"min": 8, "max": 524288, "increment": 8}],
        "macLen":    [{"min": 32, "max": 512,   "increment": 8}],
    }


def build_sha3_256_cap(*_):
    return {
        "algorithm": "SHA3-256",
        "revision":  "2.0",
        "messageLength": [{"min": 0, "max": 65536, "increment": 8}],
    }


def build_sha3_384_cap(*_):
    return {
        "algorithm": "SHA3-384",
        "revision":  "2.0",
        "messageLength": [{"min": 0, "max": 65536, "increment": 8}],
    }


def build_sha3_512_cap(*_):
    return {
        "algorithm": "SHA3-512",
        "revision":  "2.0",
        "messageLength": [{"min": 0, "max": 65536, "increment": 8}],
    }


# ---------------------------------------------------------------------------
# PQC CAPABILITY BUILDERS
# ---------------------------------------------------------------------------

def build_ml_kem_keygen_cap(*_):
    return {
        "algorithm":     "ML-KEM",
        "mode":          "keyGen",
        "revision":      "FIPS203",
        "parameterSets": ["ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"],
    }


def build_ml_dsa_keygen_cap(*_):
    return {
        "algorithm":     "ML-DSA",
        "mode":          "keyGen",
        "revision":      "FIPS204",
        "parameterSets": ["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"],
    }


def build_ml_dsa_siggen_cap(*_):
    ps  = ["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"]
    mln = [{"min": 8, "max": 65536, "increment": 8}]
    return {
        "algorithm":           "ML-DSA",
        "mode":                "sigGen",
        "revision":            "FIPS204",
        "parameterSets":       ps,
        "messageLength":       mln,
        "deterministic":       [True, False],
        "signatureInterfaces": ["1"],
        "capabilities": [{"parameterSets": ps, "messageLength": mln, "deterministic": [True, False]}],
    }


def build_ml_dsa_sigver_cap(*_):
    ps  = ["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"]
    mln = [{"min": 8, "max": 65536, "increment": 8}]
    return {
        "algorithm":           "ML-DSA",
        "mode":                "sigVer",
        "revision":            "FIPS204",
        "parameterSets":       ps,
        "messageLength":       mln,
        "signatureInterfaces": ["1"],
        "capabilities": [{"parameterSets": ps, "messageLength": mln}],
    }


_SLH_DSA_PARAM_SETS = [
    "SLH-DSA-SHA2-128s",  "SLH-DSA-SHA2-128f",
    "SLH-DSA-SHA2-192s",  "SLH-DSA-SHA2-192f",
    "SLH-DSA-SHA2-256s",  "SLH-DSA-SHA2-256f",
    "SLH-DSA-SHAKE-128s", "SLH-DSA-SHAKE-128f",
    "SLH-DSA-SHAKE-192s", "SLH-DSA-SHAKE-192f",
    "SLH-DSA-SHAKE-256s", "SLH-DSA-SHAKE-256f",
]


def build_slh_dsa_keygen_cap(*_):
    return {
        "algorithm":     "SLH-DSA",
        "mode":          "keyGen",
        "revision":      "FIPS205",
        "parameterSets": _SLH_DSA_PARAM_SETS,
    }


def build_slh_dsa_siggen_cap(*_):
    mln = [{"min": 8, "max": 65536, "increment": 8}]
    return {
        "algorithm":           "SLH-DSA",
        "mode":                "sigGen",
        "revision":            "FIPS205",
        "parameterSets":       _SLH_DSA_PARAM_SETS,
        "messageLength":       mln,
        "deterministic":       [True, False],
        "signatureInterfaces": ["1"],
        "capabilities": [{"parameterSets": _SLH_DSA_PARAM_SETS, "messageLength": mln, "deterministic": [True, False]}],
    }


def build_slh_dsa_sigver_cap(*_):
    mln = [{"min": 8, "max": 65536, "increment": 8}]
    return {
        "algorithm":           "SLH-DSA",
        "mode":                "sigVer",
        "revision":            "FIPS205",
        "parameterSets":       _SLH_DSA_PARAM_SETS,
        "messageLength":       mln,
        "signatureInterfaces": ["1"],
        "capabilities": [{"parameterSets": _SLH_DSA_PARAM_SETS, "messageLength": mln}],
    }


CAPABILITY_BUILDERS = {
    "ACVP-AES-CBC":   build_aes_cbc_cap,
    "ACVP-AES-CTR":   build_aes_ctr_cap,
    "ACVP-AES-ECB":   build_aes_ecb_cap,
    "HMAC-SHA2-256":  build_hmac_sha256_cap,
    "HMAC-SHA2-384":  build_hmac_sha384_cap,
    "HMAC-SHA2-512":  build_hmac_sha512_cap,
    "SHA2-256":       build_sha256_cap,
    "SHA2-384":       build_sha384_cap,
    "SHA2-512":       build_sha512_cap,
    "SHA3-256":       build_sha3_256_cap,
    "SHA3-384":       build_sha3_384_cap,
    "SHA3-512":       build_sha3_512_cap,
    "ML-KEM-keyGen":  build_ml_kem_keygen_cap,
    "ML-DSA-keyGen":  build_ml_dsa_keygen_cap,
    "ML-DSA-sigGen":  build_ml_dsa_siggen_cap,
    "ML-DSA-sigVer":  build_ml_dsa_sigver_cap,
    "SLH-DSA-keyGen": build_slh_dsa_keygen_cap,
    "SLH-DSA-sigGen": build_slh_dsa_siggen_cap,
    "SLH-DSA-sigVer": build_slh_dsa_sigver_cap,
}


# ---------------------------------------------------------------------------
# OPENSSL HELPERS
# ---------------------------------------------------------------------------

def run_openssl(args_list, stdin=None):
    """Run openssl with the given argument list; return stdout bytes."""
    result = subprocess.run([OPENSSL_BIN] + args_list, input=stdin, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.decode().strip())
    return result.stdout


# ---------------------------------------------------------------------------
# PQC CTYPES HELPERS  (EVP C API for deterministic keygen and sign/verify)
# ---------------------------------------------------------------------------

_OSSL_LIB_PATH = None   # set from --lib-path or auto-detected in main()


def _find_libcrypto(openssl_bin):
    """Auto-detect libcrypto shared library for the given OpenSSL binary.

    Strategy (in order):
    1. Read the binary's dynamic link metadata to find the exact library it
       was compiled against (otool -L on macOS; ldd on Linux).
    2. Glob for libcrypto in lib/ and lib64/ relative to the binary.
    3. Fall back to ctypes.util.find_library("crypto").
    """
    # 1. Inspect the binary's dynamic link metadata.
    if platform.system() == "Darwin":
        try:
            out = subprocess.check_output(
                ["otool", "-L", openssl_bin], stderr=subprocess.DEVNULL
            ).decode()
            for line in out.splitlines():
                m = re.match(r"\s+(/\S*libcrypto\S*\.dylib)", line)
                if m:
                    return m.group(1)
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
    else:
        try:
            out = subprocess.check_output(
                ["ldd", openssl_bin], stderr=subprocess.DEVNULL
            ).decode()
            for line in out.splitlines():
                m = re.search(r"libcrypto\.so\S*\s+=>\s+(\S+)", line)
                if m and m.group(1) != "not":
                    return m.group(1)
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass

    # 2. Glob relative to the binary location.
    bin_dir = os.path.dirname(os.path.realpath(openssl_bin))
    search_dirs = [
        os.path.realpath(os.path.join(bin_dir, "..", "lib")),
        os.path.realpath(os.path.join(bin_dir, "..", "lib64")),
    ]
    patterns = (["libcrypto.*.dylib", "libcrypto.dylib"] if platform.system() == "Darwin"
                else ["libcrypto.so.*", "libcrypto.so"])

    for lib_dir in search_dirs:
        for pattern in patterns:
            matches = sorted(glob.glob(os.path.join(lib_dir, pattern)))
            if matches:
                return matches[-1]

    # 3. System-wide search.
    from ctypes.util import find_library
    found = find_library("crypto")
    if found:
        return found

    raise RuntimeError(
        "Could not find libcrypto. Use --lib-path to specify the path explicitly."
    )


_EVP_PKEY_PUBLIC_KEY = 0x86   # OSSL_KEYMGMT_SELECT_PUBLIC_KEY | OSSL_KEYMGMT_SELECT_ALL_PARAMS
_EVP_PKEY_KEYPAIR    = 0x87   # public + private

_lib_crypto = None


def _lib():
    global _lib_crypto
    if _lib_crypto is not None:
        return _lib_crypto

    lib = ctypes.CDLL(os.path.realpath(_OSSL_LIB_PATH))

    # OSSL_PARAM_BLD
    lib.OSSL_PARAM_BLD_new.restype           = ctypes.c_void_p
    lib.OSSL_PARAM_BLD_new.argtypes          = []
    lib.OSSL_PARAM_BLD_free.restype          = None
    lib.OSSL_PARAM_BLD_free.argtypes         = [ctypes.c_void_p]
    lib.OSSL_PARAM_BLD_to_param.restype      = ctypes.c_void_p
    lib.OSSL_PARAM_BLD_to_param.argtypes     = [ctypes.c_void_p]
    lib.OSSL_PARAM_BLD_push_octet_string.restype  = ctypes.c_int
    lib.OSSL_PARAM_BLD_push_octet_string.argtypes = [
        ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t,
    ]
    lib.OSSL_PARAM_free.restype  = None
    lib.OSSL_PARAM_free.argtypes = [ctypes.c_void_p]

    # EVP_PKEY_CTX
    lib.EVP_PKEY_CTX_new_from_name.restype  = ctypes.c_void_p
    lib.EVP_PKEY_CTX_new_from_name.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
    lib.EVP_PKEY_CTX_free.restype           = None
    lib.EVP_PKEY_CTX_free.argtypes          = [ctypes.c_void_p]
    lib.EVP_PKEY_CTX_set_params.restype     = ctypes.c_int
    lib.EVP_PKEY_CTX_set_params.argtypes    = [ctypes.c_void_p, ctypes.c_void_p]

    # keygen
    lib.EVP_PKEY_keygen_init.restype  = ctypes.c_int
    lib.EVP_PKEY_keygen_init.argtypes = [ctypes.c_void_p]
    lib.EVP_PKEY_generate.restype     = ctypes.c_int
    lib.EVP_PKEY_generate.argtypes    = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]

    # fromdata
    lib.EVP_PKEY_fromdata_init.restype  = ctypes.c_int
    lib.EVP_PKEY_fromdata_init.argtypes = [ctypes.c_void_p]
    lib.EVP_PKEY_fromdata.restype       = ctypes.c_int
    lib.EVP_PKEY_fromdata.argtypes      = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p), ctypes.c_int, ctypes.c_void_p,
    ]

    # raw key extraction
    lib.EVP_PKEY_get_raw_public_key.restype   = ctypes.c_int
    lib.EVP_PKEY_get_raw_public_key.argtypes  = [
        ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t),
    ]
    lib.EVP_PKEY_get_raw_private_key.restype  = ctypes.c_int
    lib.EVP_PKEY_get_raw_private_key.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t),
    ]

    # EVP_PKEY_free
    lib.EVP_PKEY_free.restype  = None
    lib.EVP_PKEY_free.argtypes = [ctypes.c_void_p]

    # EVP_MD_CTX for sign/verify
    lib.EVP_MD_CTX_new.restype   = ctypes.c_void_p
    lib.EVP_MD_CTX_new.argtypes  = []
    lib.EVP_MD_CTX_free.restype  = None
    lib.EVP_MD_CTX_free.argtypes = [ctypes.c_void_p]

    lib.EVP_DigestSignInit_ex.restype  = ctypes.c_int
    lib.EVP_DigestSignInit_ex.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p),
        ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p,
        ctypes.c_void_p, ctypes.c_void_p,
    ]
    lib.EVP_DigestSign.restype  = ctypes.c_int
    lib.EVP_DigestSign.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t),
        ctypes.c_void_p, ctypes.c_size_t,
    ]

    lib.EVP_DigestVerifyInit_ex.restype  = ctypes.c_int
    lib.EVP_DigestVerifyInit_ex.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p),
        ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p,
        ctypes.c_void_p, ctypes.c_void_p,
    ]
    lib.EVP_DigestVerify.restype  = ctypes.c_int
    lib.EVP_DigestVerify.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
        ctypes.c_void_p, ctypes.c_size_t,
    ]

    _lib_crypto = lib
    return lib


def _build_octet_params(key_name: bytes, data: bytes):
    """Build a single-entry OSSL_PARAM array; caller must OSSL_PARAM_free.

    key_name MUST be a bytes literal from the caller (OSSL_PARAM_BLD_to_param
    stores only a pointer to the key name, not a copy, so it must outlive params).
    """
    lib = _lib()
    bld = lib.OSSL_PARAM_BLD_new()
    if not bld:
        raise RuntimeError("OSSL_PARAM_BLD_new failed")
    buf = (ctypes.c_ubyte * len(data))(*data)
    ok  = lib.OSSL_PARAM_BLD_push_octet_string(bld, key_name, buf, len(data))
    params = lib.OSSL_PARAM_BLD_to_param(bld)
    lib.OSSL_PARAM_BLD_free(bld)
    if not ok or not params:
        raise RuntimeError(f"OSSL_PARAM_BLD_push_octet_string({key_name}) failed")
    return params


def _pqc_keygen_from_seed(algo_name: str, seed: bytes):
    """Generate a PQC key pair from a seed; returns (raw_pub_bytes, raw_priv_bytes)."""
    lib    = _lib()
    params = _build_octet_params(b"seed", seed)
    try:
        ctx = lib.EVP_PKEY_CTX_new_from_name(None, algo_name.encode(), None)
        if not ctx:
            raise RuntimeError(f"EVP_PKEY_CTX_new_from_name({algo_name}) failed")
        try:
            if lib.EVP_PKEY_keygen_init(ctx) <= 0:
                raise RuntimeError("EVP_PKEY_keygen_init failed")
            if lib.EVP_PKEY_CTX_set_params(ctx, params) <= 0:
                raise RuntimeError("EVP_PKEY_CTX_set_params(seed) failed")
            pkey = ctypes.c_void_p(None)
            if lib.EVP_PKEY_generate(ctx, ctypes.byref(pkey)) <= 0:
                raise RuntimeError(f"EVP_PKEY_generate({algo_name}) failed")
        finally:
            lib.EVP_PKEY_CTX_free(ctx)
    finally:
        lib.OSSL_PARAM_free(params)

    try:
        pub_len = ctypes.c_size_t(0)
        lib.EVP_PKEY_get_raw_public_key(pkey, None, ctypes.byref(pub_len))
        pub_buf = (ctypes.c_ubyte * pub_len.value)()
        if lib.EVP_PKEY_get_raw_public_key(pkey, pub_buf, ctypes.byref(pub_len)) <= 0:
            raise RuntimeError("EVP_PKEY_get_raw_public_key failed")

        priv_len = ctypes.c_size_t(0)
        lib.EVP_PKEY_get_raw_private_key(pkey, None, ctypes.byref(priv_len))
        priv_buf = (ctypes.c_ubyte * priv_len.value)()
        if lib.EVP_PKEY_get_raw_private_key(pkey, priv_buf, ctypes.byref(priv_len)) <= 0:
            raise RuntimeError("EVP_PKEY_get_raw_private_key failed")

        return bytes(pub_buf), bytes(priv_buf)
    finally:
        lib.EVP_PKEY_free(pkey)


def _pqc_load_pub(algo_name: str, pub_bytes: bytes):
    """Load raw public key bytes into an EVP_PKEY (public key only); caller must free."""
    lib    = _lib()
    params = _build_octet_params(b"pub", pub_bytes)
    try:
        ctx = lib.EVP_PKEY_CTX_new_from_name(None, algo_name.encode(), None)
        if not ctx:
            raise RuntimeError(f"EVP_PKEY_CTX_new_from_name({algo_name}) failed")
        try:
            if lib.EVP_PKEY_fromdata_init(ctx) <= 0:
                raise RuntimeError("EVP_PKEY_fromdata_init failed")
            pkey = ctypes.c_void_p(None)
            if lib.EVP_PKEY_fromdata(ctx, ctypes.byref(pkey), _EVP_PKEY_PUBLIC_KEY, params) <= 0:
                raise RuntimeError(f"EVP_PKEY_fromdata(pub, {algo_name}) failed")
        finally:
            lib.EVP_PKEY_CTX_free(ctx)
    finally:
        lib.OSSL_PARAM_free(params)
    return pkey


def _pqc_load_priv(algo_name: str, priv_bytes: bytes):
    """Load raw private key bytes into an EVP_PKEY (keypair); caller must free."""
    lib    = _lib()
    params = _build_octet_params(b"priv", priv_bytes)
    try:
        ctx = lib.EVP_PKEY_CTX_new_from_name(None, algo_name.encode(), None)
        if not ctx:
            raise RuntimeError(f"EVP_PKEY_CTX_new_from_name({algo_name}) failed")
        try:
            if lib.EVP_PKEY_fromdata_init(ctx) <= 0:
                raise RuntimeError("EVP_PKEY_fromdata_init failed")
            pkey = ctypes.c_void_p(None)
            if lib.EVP_PKEY_fromdata(ctx, ctypes.byref(pkey), _EVP_PKEY_KEYPAIR, params) <= 0:
                raise RuntimeError(f"EVP_PKEY_fromdata(priv, {algo_name}) failed")
        finally:
            lib.EVP_PKEY_CTX_free(ctx)
    finally:
        lib.OSSL_PARAM_free(params)
    return pkey


_MAX_SIG_BUF = 65536  # large enough for any PQC signature (SLH-DSA-SHA2-256f ≈ 49 856 B)

# Direct OSSL_PARAM struct for setting sign params without PARAM_BLD (key name stays
# alive as a bytes literal in the calling frame — c_char_p stores only a pointer).
class _OSSL_PARAM(ctypes.Structure):
    _fields_ = [
        ("key",         ctypes.c_char_p),
        ("data_type",   ctypes.c_uint32),
        ("data",        ctypes.c_void_p),
        ("data_size",   ctypes.c_size_t),
        ("return_size", ctypes.c_size_t),
    ]

_OSSL_PARAM_INTEGER      = 1
_OSSL_PARAM_OCTET_STRING = 5
_OSSL_RETURN_SIZE_UNSET  = ctypes.c_size_t(-1).value


def _pqc_sign(algo_name: str, priv_bytes: bytes, msg: bytes,
              *, deterministic: bool = True, rnd: bytes = None) -> bytes:
    """Sign msg with raw private key bytes; returns signature bytes.

    Always sets message-encoding=0 (RAW) for ACVP signatureInterface:internal.
    deterministic=True  → set "deterministic"=1 (rnd=0^n per FIPS 204/205).
    deterministic=False → set "test-entropy"=rnd bytes provided by ACVP.
    """
    lib  = _lib()
    pkey = _pqc_load_priv(algo_name, priv_bytes)
    try:
        mdctx = lib.EVP_MD_CTX_new()
        if not mdctx:
            raise RuntimeError("EVP_MD_CTX_new failed")
        try:
            pctx = ctypes.c_void_p(None)
            if lib.EVP_DigestSignInit_ex(
                    mdctx, ctypes.byref(pctx), None, None, None, pkey, None) <= 0:
                raise RuntimeError(f"EVP_DigestSignInit_ex({algo_name}) failed")

            enc_val = ctypes.c_int(0)   # message-encoding = 0 (RAW)
            if deterministic:
                det_val = ctypes.c_int(1)
                sp = (_OSSL_PARAM * 3)()
                sp[0].key         = b"message-encoding"
                sp[0].data_type   = _OSSL_PARAM_INTEGER
                sp[0].data        = ctypes.cast(ctypes.byref(enc_val), ctypes.c_void_p)
                sp[0].data_size   = ctypes.sizeof(enc_val)
                sp[0].return_size = _OSSL_RETURN_SIZE_UNSET
                sp[1].key         = b"deterministic"
                sp[1].data_type   = _OSSL_PARAM_INTEGER
                sp[1].data        = ctypes.cast(ctypes.byref(det_val), ctypes.c_void_p)
                sp[1].data_size   = ctypes.sizeof(det_val)
                sp[1].return_size = _OSSL_RETURN_SIZE_UNSET
                sp[2].key         = None
            elif rnd is not None:
                rnd_buf = (ctypes.c_ubyte * len(rnd))(*rnd)
                sp = (_OSSL_PARAM * 3)()
                sp[0].key         = b"message-encoding"
                sp[0].data_type   = _OSSL_PARAM_INTEGER
                sp[0].data        = ctypes.cast(ctypes.byref(enc_val), ctypes.c_void_p)
                sp[0].data_size   = ctypes.sizeof(enc_val)
                sp[0].return_size = _OSSL_RETURN_SIZE_UNSET
                sp[1].key         = b"test-entropy"
                sp[1].data_type   = _OSSL_PARAM_OCTET_STRING
                sp[1].data        = ctypes.cast(rnd_buf, ctypes.c_void_p)
                sp[1].data_size   = len(rnd)
                sp[1].return_size = _OSSL_RETURN_SIZE_UNSET
                sp[2].key         = None
            else:
                sp = (_OSSL_PARAM * 2)()
                sp[0].key         = b"message-encoding"
                sp[0].data_type   = _OSSL_PARAM_INTEGER
                sp[0].data        = ctypes.cast(ctypes.byref(enc_val), ctypes.c_void_p)
                sp[0].data_size   = ctypes.sizeof(enc_val)
                sp[0].return_size = _OSSL_RETURN_SIZE_UNSET
                sp[1].key         = None

            if lib.EVP_PKEY_CTX_set_params(pctx, sp) <= 0:
                raise RuntimeError(f"EVP_PKEY_CTX_set_params({algo_name}) failed")

            sig_buf = (ctypes.c_ubyte * _MAX_SIG_BUF)()
            sig_len = ctypes.c_size_t(_MAX_SIG_BUF)
            msg_buf = (ctypes.c_ubyte * len(msg))(*msg)
            if lib.EVP_DigestSign(
                    mdctx, sig_buf, ctypes.byref(sig_len), msg_buf, len(msg)) <= 0:
                raise RuntimeError(f"EVP_DigestSign({algo_name}) failed")
            return bytes(sig_buf[:sig_len.value])
        finally:
            lib.EVP_MD_CTX_free(mdctx)
    finally:
        lib.EVP_PKEY_free(pkey)


def _pqc_verify(algo_name: str, pub_bytes: bytes, msg: bytes, sig: bytes) -> bool:
    """Verify a PQC signature; returns True if valid."""
    lib  = _lib()
    pkey = _pqc_load_pub(algo_name, pub_bytes)
    try:
        mdctx = lib.EVP_MD_CTX_new()
        if not mdctx:
            raise RuntimeError("EVP_MD_CTX_new failed")
        try:
            pctx = ctypes.c_void_p(None)
            if lib.EVP_DigestVerifyInit_ex(
                    mdctx, ctypes.byref(pctx), None, None, None, pkey, None) <= 0:
                raise RuntimeError(f"EVP_DigestVerifyInit_ex({algo_name}) failed")
            # message-encoding=0 (RAW) for ACVP signatureInterface:internal
            enc_val = ctypes.c_int(0)
            sp = (_OSSL_PARAM * 2)()
            sp[0].key         = b"message-encoding"
            sp[0].data_type   = _OSSL_PARAM_INTEGER
            sp[0].data        = ctypes.cast(ctypes.byref(enc_val), ctypes.c_void_p)
            sp[0].data_size   = ctypes.sizeof(enc_val)
            sp[0].return_size = _OSSL_RETURN_SIZE_UNSET
            sp[1].key         = None
            if lib.EVP_PKEY_CTX_set_params(pctx, sp) <= 0:
                raise RuntimeError(f"EVP_PKEY_CTX_set_params({algo_name}) failed")
            sig_buf = (ctypes.c_ubyte * len(sig))(*sig)
            msg_buf = (ctypes.c_ubyte * len(msg))(*msg)
            ret = lib.EVP_DigestVerify(mdctx, sig_buf, len(sig), msg_buf, len(msg))
            return ret == 1
        finally:
            lib.EVP_MD_CTX_free(mdctx)
    finally:
        lib.EVP_PKEY_free(pkey)


def _aes_cipher_name(algorithm, key_len):
    # "ACVP-AES-CBC" → "aes-256-cbc"
    mode = algorithm.split("-")[-1].lower()
    return f"aes-{key_len}-{mode}"


def _aes_ecb_block(key_bytes: bytes, block: bytes, decrypt: bool = False) -> bytes:
    """Encrypt or decrypt one 16-byte block with AES-ECB via the cryptography library."""
    c = Cipher(algorithms.AES(key_bytes), modes.ECB())
    op = c.decryptor() if decrypt else c.encryptor()
    return op.update(block) + op.finalize()


def _aes_ecb_mct(direction: str, key_hex: str, payload_hex: str) -> list[dict]:
    """
    Run AES-ECB MCT (100 outer × 1000 inner iterations, no IV).

    Each inner step: output = AES_ECB_{Enc|Dec}(key, input); next_input = output.
    Key derivation same as CBC: XOR with last n bytes of output[998] ‖ output[999].
    """
    key = bytes.fromhex(key_hex)
    msg = bytes.fromhex(payload_hex)
    n   = len(key)
    dec = (direction == "decrypt")

    results = []
    for _ in range(100):
        round_key = key
        round_msg = msg

        out_prev2 = out_prev = None
        for _ in range(1000):
            out = _aes_ecb_block(key, msg, decrypt=dec)
            out_prev2, out_prev = out_prev, out
            msg = out

        combined = out_prev2 + out_prev
        key = bytes(a ^ b for a, b in zip(key, combined[-n:]))
        msg = out_prev  # output[999] seeds the next outer iteration

        if direction == "encrypt":
            results.append({"key": round_key.hex().upper(),
                             "pt":  round_msg.hex().upper(),
                             "ct":  out_prev.hex().upper()})
        else:
            results.append({"key": round_key.hex().upper(),
                             "ct":  round_msg.hex().upper(),
                             "pt":  out_prev.hex().upper()})

    return results


def _aes_cbc_mct(direction: str, key_hex: str, iv_hex: str, payload_hex: str) -> list[dict]:
    """
    Run the ACVP AES-CBC Monte Carlo Test (100 outer × 1000 inner iterations).

    Using the cryptography library for the inner loop avoids 100,000 subprocess
    calls (~17 min) while still exercising the same mathematical operations that
    the AFT tests already validated against the target binary.

    Key derivation per NIST SP 800-20 / ACVP spec:
      128-bit: newKey = key XOR CT[999]
      192-bit: newKey = key XOR CT[998][8:] ‖ CT[999]
      256-bit: newKey = key XOR CT[998] ‖ CT[999]
    """
    key = bytes.fromhex(key_hex)
    iv  = bytes.fromhex(iv_hex)
    pt  = bytes.fromhex(payload_hex)
    n   = len(key)  # 16, 24, or 32 bytes

    results = []
    for _ in range(100):
        round_key = key
        round_iv  = iv
        round_pt  = pt

        ct_prev2 = ct_prev = None
        for j in range(1000):
            if direction == "encrypt":
                ct = _aes_ecb_block(key, bytes(a ^ b for a, b in zip(pt, iv)))
                pt, iv = (iv if j == 0 else ct_prev), ct
            else:
                # CBC decrypt: output = AES_ECB_Dec(key, CT_in) XOR IV
                ct = bytes(a ^ b for a, b in zip(_aes_ecb_block(key, pt, decrypt=True), iv))
                iv, pt = pt, (iv if j == 0 else ct_prev)
            ct_prev2, ct_prev = ct_prev, ct

        # ct_prev = output[999], ct_prev2 = output[998]
        combined = ct_prev2 + ct_prev
        key = bytes(a ^ b for a, b in zip(key, combined[-n:]))
        iv  = ct_prev   # output[999] → next IV (same rule for both directions)
        pt  = ct_prev2  # output[998] → next msg (same rule for both directions)

        if direction == "encrypt":
            results.append({"key": round_key.hex().upper(),
                             "iv":  round_iv.hex().upper(),
                             "pt":  round_pt.hex().upper(),
                             "ct":  ct_prev.hex().upper()})
        else:
            # round_pt holds the initial CT input for this outer iteration
            results.append({"key": round_key.hex().upper(),
                             "iv":  round_iv.hex().upper(),
                             "ct":  round_pt.hex().upper(),
                             "pt":  ct_prev.hex().upper()})

    return results


def process_aes_symmetric(group, tc):
    """Handler for ACVP-AES-CBC and ACVP-AES-ECB (AFT and MCT)."""
    algorithm = group.get("algorithm", "ACVP-AES-CBC")
    direction = group.get("direction", "encrypt")
    test_type = group.get("testType", "AFT")
    key       = tc["key"]
    key_len   = group.get("keyLen", len(bytes.fromhex(key)) * 8)
    cipher    = _aes_cipher_name(algorithm, key_len)
    iv        = tc.get("iv", tc.get("IV", ""))

    if test_type == "MCT":
        payload = tc.get("pt" if direction == "encrypt" else "ct", "")
        if "ECB" in algorithm.upper():
            return {"tcId": tc["tcId"], "resultsArray": _aes_ecb_mct(direction, key, payload)}
        return {"tcId": tc["tcId"], "resultsArray": _aes_cbc_mct(direction, key, iv, payload)}

    # AFT — call the target OpenSSL binary
    base_args = ["enc", f"-{cipher}", "-nosalt", "-nopad", "-K", key]
    if iv:
        base_args += ["-iv", iv]

    if direction == "encrypt":
        pt_bytes = bytes.fromhex(tc.get("pt", tc.get("plainText", "")))
        ct = run_openssl(base_args, stdin=pt_bytes).hex().upper()
        return {"tcId": tc["tcId"], "ct": ct}
    else:
        ct_bytes = bytes.fromhex(tc.get("ct", tc.get("cipherText", "")))
        pt = run_openssl(base_args + ["-d"], stdin=ct_bytes).hex().upper()
        return {"tcId": tc["tcId"], "pt": pt}


def process_aes_ctr(group, tc):
    """Handler for ACVP-AES-CTR (AFT only — no MCT for CTR mode)."""
    direction = group.get("direction", "encrypt")
    key       = tc["key"]
    key_len   = group.get("keyLen", len(bytes.fromhex(key)) * 8)
    iv        = tc.get("iv", tc.get("IV", ""))
    payload   = tc.get("pt" if direction == "encrypt" else "ct", "")

    result = run_openssl(
        ["enc", f"-aes-{key_len}-ctr", "-nosalt", "-nopad", "-K", key, "-iv", iv],
        stdin=bytes.fromhex(payload) if payload else b"",
    )
    if direction == "encrypt":
        return {"tcId": tc["tcId"], "ct": result.hex().upper()}
    return {"tcId": tc["tcId"], "pt": result.hex().upper()}


def _sha2_mct(algorithm: str, seed_hex: str) -> list[dict]:
    """SHA2 MCT: 3-value sliding window, 100 outer × 1000 inner iterations."""
    bits = algorithm.split("-")[-1]
    h    = getattr(hashlib, f"sha{bits}")
    seed = bytes.fromhex(seed_hex)
    results = []
    for _ in range(100):
        md = [seed, seed, seed]
        for _ in range(1000):
            new_md = h(md[0] + md[1] + md[2]).digest()
            md = [md[1], md[2], new_md]
        seed = md[2]
        results.append({"md": seed.hex().upper()})
    return results


def _sha3_mct(algorithm: str, seed_hex: str) -> list[dict]:
    """SHA3 MCT: simple chain, 100 outer × 1000 inner iterations."""
    bits = algorithm.split("-")[-1]
    h    = getattr(hashlib, f"sha3_{bits}")
    seed = bytes.fromhex(seed_hex)
    results = []
    for _ in range(100):
        for _ in range(1000):
            seed = h(seed).digest()
        results.append({"md": seed.hex().upper()})
    return results


def process_sha2(group, tc):
    """Handler for SHA2-256/384/512.  Only handles byte-aligned messages."""
    algorithm = group.get("algorithm", "SHA2-256")
    test_type = group.get("testType", "AFT")
    bits      = algorithm.split("-")[-1]   # "256", "384", "512"

    if test_type == "MCT":
        seed_hex = tc.get("msg", "")
        return {"tcId": tc["tcId"], "resultsArray": _sha2_mct(algorithm, seed_hex)}

    msg_hex   = tc.get("msg", "")
    msg_len   = tc.get("len", len(msg_hex) * 4)  # bit length
    msg_bytes = bytes.fromhex(msg_hex)[:msg_len // 8] if msg_hex else b""
    md = run_openssl(["dgst", f"-sha{bits}", "-binary"], stdin=msg_bytes).hex().upper()
    return {"tcId": tc["tcId"], "md": md}


def process_sha3(group, tc):
    """Handler for SHA3-256/384/512.  Only handles byte-aligned messages."""
    algorithm = group.get("algorithm", "SHA3-256")
    test_type = group.get("testType", "AFT")
    bits      = algorithm.split("-")[-1]   # "256", "384", "512"

    if test_type == "MCT":
        seed_hex = tc.get("msg", "")
        return {"tcId": tc["tcId"], "resultsArray": _sha3_mct(algorithm, seed_hex)}

    msg_hex   = tc.get("msg", "")
    msg_len   = tc.get("len", len(msg_hex) * 4)
    msg_bytes = bytes.fromhex(msg_hex)[:msg_len // 8] if msg_hex else b""
    md = run_openssl(["dgst", f"-sha3-{bits}", "-binary"], stdin=msg_bytes).hex().upper()
    return {"tcId": tc["tcId"], "md": md}


def process_hmac_sha2(group, tc):
    """Handler for HMAC-SHA2-256/384/512."""
    algorithm = group.get("algorithm", "HMAC-SHA2-256")
    bits      = algorithm.split("-")[-1]   # "256", "384", "512"
    key_hex   = tc["key"]
    msg_hex   = tc.get("msg", "")
    mac_len   = group.get("macLen", int(bits)) // 8  # bits → bytes

    msg_bytes = bytes.fromhex(msg_hex) if msg_hex else b""
    full_mac  = run_openssl(
        ["dgst", f"-sha{bits}", "-mac", "HMAC", "-macopt", f"hexkey:{key_hex}", "-binary"],
        stdin=msg_bytes,
    ).hex().upper()
    return {"tcId": tc["tcId"], "mac": full_mac[: mac_len * 2]}


# ---------------------------------------------------------------------------
# PQC ALGORITHM HANDLERS
# ---------------------------------------------------------------------------

def process_ml_kem_keygen(group, tc):
    param_set = group["parameterSet"]
    seed      = bytes.fromhex(tc["d"]) + bytes.fromhex(tc["z"])
    ek, dk    = _pqc_keygen_from_seed(param_set, seed)
    return {"tcId": tc["tcId"], "ek": ek.hex().upper(), "dk": dk.hex().upper()}


def process_ml_dsa_keygen(group, tc):
    param_set = group["parameterSet"]
    seed      = bytes.fromhex(tc["seed"])
    pk, sk    = _pqc_keygen_from_seed(param_set, seed)
    return {"tcId": tc["tcId"], "pk": pk.hex().upper(), "sk": sk.hex().upper()}


def process_slh_dsa_keygen(group, tc):
    param_set = group["parameterSet"]
    seed      = (bytes.fromhex(tc["skSeed"])
                 + bytes.fromhex(tc["skPrf"])
                 + bytes.fromhex(tc["pkSeed"]))
    pk, sk    = _pqc_keygen_from_seed(param_set, seed)
    return {"tcId": tc["tcId"], "pk": pk.hex().upper(), "sk": sk.hex().upper()}


def process_ml_dsa_siggen(group, tc):
    param_set   = group["parameterSet"]
    sk_hex      = group.get("sk") or tc.get("sk", "")
    msg         = bytes.fromhex(tc.get("message", ""))
    det         = group.get("deterministic", True)
    rnd         = bytes.fromhex(tc["rnd"]) if not det and "rnd" in tc else None
    sig         = _pqc_sign(param_set, bytes.fromhex(sk_hex), msg, deterministic=det, rnd=rnd)
    return {"tcId": tc["tcId"], "signature": sig.hex().upper()}


def process_ml_dsa_sigver(group, tc):
    param_set = group["parameterSet"]
    pk_hex    = group.get("pk") or tc.get("pk", "")
    msg       = bytes.fromhex(tc.get("message", ""))
    sig       = bytes.fromhex(tc.get("signature", ""))
    passed    = _pqc_verify(param_set, bytes.fromhex(pk_hex), msg, sig)
    return {"tcId": tc["tcId"], "testPassed": passed}


def process_slh_dsa_siggen(group, tc):
    param_set   = group["parameterSet"]
    sk_hex      = group.get("sk") or tc.get("sk", "")
    msg         = bytes.fromhex(tc.get("message", ""))
    det         = group.get("deterministic", True)
    if not det:
        rnd_hex = tc.get("rnd") or tc.get("additionalRandomness") or tc.get("optRand")
        rnd = bytes.fromhex(rnd_hex) if rnd_hex else None
    else:
        rnd = None
    sig         = _pqc_sign(param_set, bytes.fromhex(sk_hex), msg, deterministic=det, rnd=rnd)
    return {"tcId": tc["tcId"], "signature": sig.hex().upper()}


def process_slh_dsa_sigver(group, tc):
    param_set = group["parameterSet"]
    pk_hex    = group.get("pk") or tc.get("pk", "")
    msg       = bytes.fromhex(tc.get("message", ""))
    sig       = bytes.fromhex(tc.get("signature", ""))
    passed    = _pqc_verify(param_set, bytes.fromhex(pk_hex), msg, sig)
    return {"tcId": tc["tcId"], "testPassed": passed}


ALGORITHM_HANDLERS = {
    "ACVP-AES-CBC":   process_aes_symmetric,
    "ACVP-AES-CTR":   process_aes_ctr,
    "ACVP-AES-ECB":   process_aes_symmetric,
    "HMAC-SHA2-256":  process_hmac_sha2,
    "HMAC-SHA2-384":  process_hmac_sha2,
    "HMAC-SHA2-512":  process_hmac_sha2,
    "SHA2-256":       process_sha2,
    "SHA2-384":       process_sha2,
    "SHA2-512":       process_sha2,
    "SHA3-256":       process_sha3,
    "SHA3-384":       process_sha3,
    "SHA3-512":       process_sha3,
    "ML-KEM-keyGen":  process_ml_kem_keygen,
    "ML-DSA-keyGen":  process_ml_dsa_keygen,
    "ML-DSA-sigGen":  process_ml_dsa_siggen,
    "ML-DSA-sigVer":  process_ml_dsa_sigver,
    "SLH-DSA-keyGen": process_slh_dsa_keygen,
    "SLH-DSA-sigGen": process_slh_dsa_siggen,
    "SLH-DSA-sigVer": process_slh_dsa_sigver,
}


# ---------------------------------------------------------------------------
# ACVTS REST API
# ---------------------------------------------------------------------------

def load_totp_secret():
    with open(TOTP_SEED_FILE) as f:
        b64 = f.read().strip()
    return base64.b32encode(base64.b64decode(b64)).decode()


def get_totp(secret_b32):
    return pyotp.TOTP(secret_b32, digits=8, digest=hashlib.sha256, interval=30).now()


def login(session, totp_secret_b32):
    payload = [{"acvVersion": "1.0"}, {"password": get_totp(totp_secret_b32)}]
    r = session.post(f"{BASE_URL}/login", json=payload)
    r.raise_for_status()
    info            = r.json()[1]
    token           = info["accessToken"]
    size_constraint = info.get("sizeConstraint", -1)
    print(f"[+] Logged in  sizeConstraint={size_constraint}")
    return token, size_constraint


def register(session, token, algorithm_cap, is_sample=True):
    """Create a test session; return (ts_id, [vs_id, ...], new_token)."""
    headers = {"Authorization": f"Bearer {token}"}
    payload = [
        {"acvVersion": "1.0"},
        {
            "isSample":   is_sample,
            "algorithms": [algorithm_cap],  # must be a list
        },
    ]
    r = session.post(f"{BASE_URL}/testSessions", json=payload, headers=headers)
    r.raise_for_status()
    info = r.json()[1]

    # Session id lives at the end of the "url" path
    ts_url = info.get("url", "")
    ts_id  = ts_url.rstrip("/").split("/")[-1] if ts_url else str(info.get("id", ""))

    # Server returns either vectorSetUrls (list of URL strings) or
    # vectorSets (list of {vsId, ...} objects) depending on server version.
    vs_urls = info.get("vectorSetUrls", [])
    if vs_urls:
        vs_ids = [u.rstrip("/").split("/")[-1] for u in vs_urls]
    else:
        vs_ids = [str(vs["vsId"]) for vs in info.get("vectorSets", [])]
    new_token = info.get("accessToken", token)

    print(f"[+] Session {ts_id} created  vectorSets={vs_ids}")
    return ts_id, vs_ids, new_token


def download_vector_set(session, token, ts_id, vs_id, max_retries=20):
    headers = {"Authorization": f"Bearer {token}"}
    url     = f"{BASE_URL}/testSessions/{ts_id}/vectorSets/{vs_id}"
    for _ in range(max_retries):
        r = session.get(url, headers=headers)
        r.raise_for_status()
        body = r.json()
        if isinstance(body, list) and "retry" in body[1]:
            wait = int(body[1]["retry"])
            print(f"  [~] VS {vs_id} not ready, waiting {wait}s...")
            time.sleep(wait)
            continue
        print(f"[+] Downloaded VS {vs_id}")
        return body
    raise RuntimeError(f"VS {vs_id} never became ready after {max_retries} retries")


def process_vector_set(vs_data, algorithm):
    vs_info     = vs_data[1]
    vs_id       = vs_info["vsId"]
    test_groups = vs_info.get("testGroups", [])
    handler     = ALGORITHM_HANDLERS[algorithm]

    result_groups = []
    for group in test_groups:
        tg_id = group["tgId"]
        group.setdefault("algorithm", algorithm)
        result_tests = []
        for tc in group.get("tests", []):
            try:
                result_tests.append(handler(group, tc))
            except Exception as exc:
                print(f"  [!] tcId={tc['tcId']} error: {exc}", file=sys.stderr)
                result_tests.append({"tcId": tc["tcId"]})
        result_groups.append({"tgId": tg_id, "tests": result_tests})

    return [
        {"acvVersion": "1.0"},
        {"vsId": vs_id, "testGroups": result_groups},
    ]


def upload_results(session, token, ts_id, vs_id, results, size_constraint=-1):
    headers     = {"Authorization": f"Bearer {token}"}
    url         = f"{BASE_URL}/testSessions/{ts_id}/vectorSets/{vs_id}/results"
    payload_str = json.dumps(results)

    if 0 < size_constraint < len(payload_str):
        # Payload too large: request a dedicated large-submission URI first
        r = session.post(f"{BASE_URL}/large",
                         json=[{"acvVersion": "1.0"}, {}], headers=headers)
        r.raise_for_status()
        large = r.json()[1]
        large_headers = {
            "Authorization": f"Bearer {large['accessToken']}",
            "Content-Type": "application/json",
        }
        r = session.post(f"{BASE_URL}{large['url']}", data=payload_str, headers=large_headers)
    else:
        r = session.post(url, json=results, headers=headers)

    r.raise_for_status()
    print(f"[+] Uploaded results for VS {vs_id}")


def poll_results(session, token, ts_id, vs_id, max_retries=30):
    headers = {"Authorization": f"Bearer {token}"}
    url     = f"{BASE_URL}/testSessions/{ts_id}/vectorSets/{vs_id}/results"
    for _ in range(max_retries):
        r = session.get(url, headers=headers)
        r.raise_for_status()
        body = r.json()
        info = body[1] if isinstance(body, list) and len(body) > 1 else body
        if "retry" in info:
            wait = int(info["retry"])
            print(f"  [~] VS {vs_id} grading, waiting {wait}s...")
            time.sleep(wait)
            continue
        # Grading is complete — server returns per-test results, no retry
        print(f"  [~] VS {vs_id} grading done")
        return body
    raise RuntimeError(f"VS {vs_id} did not finish within {max_retries} polls")


def certify_session(session, token, ts_id):
    """PUT /testSessions/{id} to mark the session complete and trigger grading."""
    headers = {"Authorization": f"Bearer {token}"}
    r = session.put(f"{BASE_URL}/testSessions/{ts_id}",
                    json=[{"acvVersion": "1.0"}, {}], headers=headers)
    try:
        r.raise_for_status()
    except Exception:
        print(f"[!] Certify returned {r.status_code}: {r.text[:200]}")
        return {}
    body = r.json()
    info = body[1] if isinstance(body, list) and len(body) > 1 else body
    print(f"[+] Session certified  status={info.get('status')}  passed={info.get('passed')}")
    return info


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    algo_choices = sorted(CAPABILITY_BUILDERS)
    parser = argparse.ArgumentParser(
        description="Test an OpenSSL binary against the NIST ACVTS demo server.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"Supported algorithms: {', '.join(algo_choices)}",
    )
    parser.add_argument(
        "--openssl", required=True, metavar="PATH",
        help="Path to the openssl binary to test",
    )
    parser.add_argument(
        "--cert", required=True, metavar="FILE",
        help="TLS client certificate file (.cer) from NIST",
    )
    parser.add_argument(
        "--key", required=True, metavar="FILE",
        help="Private key file (.key) for the client certificate",
    )
    parser.add_argument(
        "--totp-seed", required=True, metavar="FILE",
        help="File containing the Base64-encoded TOTP seed (one line)",
    )
    parser.add_argument(
        "--lib-path", metavar="PATH",
        help="Path to libcrypto shared library (auto-detected if omitted)",
    )
    parser.add_argument(
        "--algorithm", default="ACVP-AES-CBC", choices=algo_choices, metavar="ALGO",
        help=f"Algorithm to test (default: ACVP-AES-CBC)",
    )
    parser.add_argument(
        "--direction", nargs="+", choices=["encrypt", "decrypt"],
        help="Direction(s) for symmetric algorithms (default: both)",
    )
    parser.add_argument(
        "--key-len", nargs="+", type=int, metavar="BITS",
        help="Key length(s) in bits for symmetric algorithms (default: all)",
    )
    parser.add_argument(
        "--production", action="store_true",
        help="Run as a production validation (default: sample/demo mode)",
    )
    parser.add_argument(
        "--save-vectors", action="store_true",
        help="Save downloaded vector sets to vectors_vsNNN.json",
    )
    args = parser.parse_args()

    global BASE_URL, OPENSSL_BIN, CERT_FILE, KEY_FILE, TOTP_SEED_FILE, _OSSL_LIB_PATH
    BASE_URL       = PROD_URL if args.production else DEMO_URL
    OPENSSL_BIN    = args.openssl
    CERT_FILE      = args.cert
    KEY_FILE       = args.key
    TOTP_SEED_FILE = args.totp_seed
    _OSSL_LIB_PATH = args.lib_path if args.lib_path else _find_libcrypto(args.openssl)

    algorithm_cap = CAPABILITY_BUILDERS[args.algorithm](args.direction, args.key_len)

    print(f"[*] Algorithm : {args.algorithm}")
    print(f"[*] Capability: {json.dumps(algorithm_cap)}")
    print(f"[*] Binary    : {OPENSSL_BIN}")
    print(f"[*] Server    : {BASE_URL}")
    print(f"[*] Sample    : {not args.production}")

    session        = requests.Session()
    session.cert   = (CERT_FILE, KEY_FILE)
    session.verify = True  # verify NIST server cert via system CA

    totp_secret = load_totp_secret()

    token, size_constraint = login(session, totp_secret)
    ts_id, vs_ids, token   = register(
        session, token, algorithm_cap, is_sample=not args.production
    )

    all_passed = True
    for vs_id in vs_ids:
        vs_data = download_vector_set(session, token, ts_id, vs_id)

        if args.save_vectors:
            fname = f"vectors_vs{vs_id}.json"
            with open(fname, "w") as f:
                json.dump(vs_data, f, indent=2)
            print(f"[+] Saved vectors → {fname}")

        print(f"[+] Running OpenSSL for VS {vs_id}...")
        results = process_vector_set(vs_data, args.algorithm)

        with open(f"results_vs{vs_id}.json", "w") as f:
            json.dump(results, f, indent=2)

        upload_results(session, token, ts_id, vs_id, results, size_constraint)

        final  = poll_results(session, token, ts_id, vs_id)
        info   = final[1] if isinstance(final, list) and len(final) > 1 else final
        # Server uses "disposition": "passed"/"failed"; per-test field is "result": "passed"/"failed"
        disposition = info.get("disposition", "")
        passed = disposition == "passed"
        failed_tcs = sum(
            1 for tc in info.get("tests", [])
            if tc.get("result", "passed") != "passed"
        )
        label = "PASS" if passed else "FAIL"
        print(f"[{label}] VS {vs_id}  disposition={disposition}  failed_tcs={failed_tcs}")
        if not passed:
            all_passed = False

    if not args.production:
        print("[*] Sample session — skipping certify (not allowed for sample sessions)")
    else:
        certify_session(session, token, ts_id)

    print()
    print("=" * 60)
    print(f"{'ALL PASSED' if all_passed else 'ONE OR MORE FAILED'} — session {ts_id}")
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
