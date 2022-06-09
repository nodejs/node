"""Utility functions to parse version numbers from each of Node's dependencies"""

from pathlib import Path
import re


def get_package_json_version(path: Path) -> str:
    with open(path, "r") as f:
        matches = re.search('"version": "(?P<version>.*)"', f.read())
        if matches is None:
            raise RuntimeError(f"Error extracting version number from {path}")
        return matches.groupdict()["version"]


def get_acorn_version() -> str:
    return get_package_json_version(Path("../../deps/acorn/acorn/package.json"))


def get_brotli_version() -> str:
    with open("../../deps/brotli/c/common/version.h", "r") as f:
        matches = re.search("#define BROTLI_VERSION (?P<version>.*)", f.read())
        if matches is None:
            raise RuntimeError("Error extracting version number for brotli")
        hex_version = matches.groupdict()["version"]
        major_version = int(hex_version, 16) >> 24
        minor_version = int(hex_version, 16) >> 12 & 0xFF
        patch_version = int(hex_version, 16) & 0xFFFFF
        return f"{major_version}.{minor_version}.{patch_version}"


def get_c_ares_version() -> str:
    with open("../../deps/cares/include/ares_version.h", "r") as f:
        matches = re.search('#define ARES_VERSION_STR "(?P<version>.*)"', f.read())
        if matches is None:
            raise RuntimeError("Error extracting version number for c-ares")
        return matches.groupdict()["version"]


def get_cjs_lexer_version() -> str:
    return get_package_json_version(Path("../../deps/cjs-module-lexer/package.json"))


def get_corepack_version() -> str:
    return get_package_json_version(Path("../../deps/corepack/package.json"))


def get_icu_version() -> str:
    with open("../../deps/icu-small/source/common/unicode/uvernum.h", "r") as f:
        matches = re.search('#define U_ICU_VERSION "(?P<version>.*)"', f.read())
        if matches is None:
            raise RuntimeError("Error extracting version number for ICU")
        return matches.groupdict()["version"]


def get_llhttp_version() -> str:
    with open("../../deps/llhttp/include/llhttp.h", "r") as f:
        matches = re.search(
            "#define LLHTTP_VERSION_MAJOR (?P<major>.*)\n"
            "#define LLHTTP_VERSION_MINOR (?P<minor>.*)\n"
            "#define LLHTTP_VERSION_PATCH (?P<patch>.*)",
            f.read(),
            re.MULTILINE,
        )
        if matches is None:
            raise RuntimeError("Error extracting version number for llhttp")
        versions = matches.groupdict()
        return f"{versions['major']}.{versions['minor']}.{versions['patch']}"


def get_nghttp2_version() -> str:
    with open("../../deps/nghttp2/lib/includes/nghttp2/nghttp2ver.h", "r") as f:
        matches = re.search('#define NGHTTP2_VERSION "(?P<version>.*)"', f.read())
        if matches is None:
            raise RuntimeError("Error extracting version number for nghttp2")
        return matches.groupdict()["version"]


def get_ngtcp2_version() -> str:
    with open("../../deps/ngtcp2/ngtcp2/lib/includes/ngtcp2/version.h", "r") as f:
        matches = re.search('#define NGTCP2_VERSION "(?P<version>.*)"', f.read())
        if matches is None:
            raise RuntimeError("Error extracting version number for ngtcp2")
        return matches.groupdict()["version"]


def get_nghttp3_version() -> str:
    with open("../../deps/ngtcp2/nghttp3/lib/includes/nghttp3/version.h", "r") as f:
        matches = re.search('#define NGHTTP3_VERSION "(?P<version>.*)"', f.read())
        if matches is None:
            raise RuntimeError("Error extracting version number for nghttp3")
        return matches.groupdict()["version"]


def get_npm_version() -> str:
    return get_package_json_version(Path("../../deps/npm/package.json"))


def get_openssl_version() -> str:
    with open("../../deps/openssl/openssl/VERSION.dat", "r") as f:
        matches = re.search(
            "MAJOR=(?P<major>.*)\n" "MINOR=(?P<minor>.*)\n" "PATCH=(?P<patch>.*)",
            f.read(),
            re.MULTILINE,
        )
        if matches is None:
            raise RuntimeError("Error extracting version number for openssl")
        versions = matches.groupdict()
        return f"{versions['major']}.{versions['minor']}.{versions['patch']}"


def get_undici_version() -> str:
    return get_package_json_version(Path("../../deps/undici/src/package.json"))


def get_libuv_version() -> str:
    with open("../../deps/uv/include/uv/version.h", "r") as f:
        matches = re.search(
            "#define UV_VERSION_MAJOR (?P<major>.*)\n"
            "#define UV_VERSION_MINOR (?P<minor>.*)\n"
            "#define UV_VERSION_PATCH (?P<patch>.*)",
            f.read(),
            re.MULTILINE,
        )
        if matches is None:
            raise RuntimeError("Error extracting version number for libuv")
        versions = matches.groupdict()
        return f"{versions['major']}.{versions['minor']}.{versions['patch']}"


def get_uvwasi_version() -> str:
    with open("../../deps/uvwasi/include/uvwasi.h", "r") as f:
        matches = re.search(
            "#define UVWASI_VERSION_MAJOR (?P<major>.*)\n"
            "#define UVWASI_VERSION_MINOR (?P<minor>.*)\n"
            "#define UVWASI_VERSION_PATCH (?P<patch>.*)",
            f.read(),
            re.MULTILINE,
        )
        if matches is None:
            raise RuntimeError("Error extracting version number for uvwasi")
        versions = matches.groupdict()
        return f"{versions['major']}.{versions['minor']}.{versions['patch']}"


def get_v8_version() -> str:
    with open("../../deps/v8/include/v8-version.h", "r") as f:
        matches = re.search(
            "#define V8_MAJOR_VERSION (?P<major>.*)\n"
            "#define V8_MINOR_VERSION (?P<minor>.*)\n"
            "#define V8_BUILD_NUMBER (?P<build>.*)\n"
            "#define V8_PATCH_LEVEL (?P<patch>.*)\n",
            f.read(),
            re.MULTILINE,
        )
        if matches is None:
            raise RuntimeError("Error extracting version number for v8")
        versions = matches.groupdict()
        patch_suffix = "" if versions["patch"] == "0" else f".{versions['patch']}"
        return (
            f"{versions['major']}.{versions['minor']}.{versions['build']}{patch_suffix}"
        )


def get_zlib_version() -> str:
    with open("../../deps/zlib/zlib.h", "r") as f:
        matches = re.search('#define ZLIB_VERSION "(?P<version>.*)"', f.read())
        if matches is None:
            raise RuntimeError("Error extracting version number for zlib")
        return matches.groupdict()["version"]
