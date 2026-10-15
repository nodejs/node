#!/usr/bin/env python3
# Generates a CycloneDX 1.5 Software Bill of Materials (SBOM) for all
# bundled runtime dependencies of the Node.js binary.
#
# Usage:
#   python tools/generate-sbom.py [--output PATH]
#   python tools/generate-sbom.py --validate [--output PATH]
#
# Output defaults to out/Release/node-v{version}-sbom.json.
# Requires ./configure to have been run first (needs config.gypi).

import argparse
import datetime
import glob
import json
import re
import sys
import uuid
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent

# ---------------------------------------------------------------------------
# config.gypi parsing
# ---------------------------------------------------------------------------

def parse_config_gypi(path):
    """Read config.gypi and return the variables dict."""
    p = Path(path)
    if not p.exists():
        print(
            f'error: {p} not found.\n'
            'Run ./configure first to generate the build configuration.',
            file=sys.stderr,
        )
        sys.exit(1)
    text = p.read_text(encoding='utf-8')
    # Strip the leading "# Do not edit..." comment line before JSON parsing.
    lines = text.splitlines()
    json_text = '\n'.join(line for line in lines if not line.startswith('#'))
    data = json.loads(json_text)
    return data.get('variables', {})


# ---------------------------------------------------------------------------
# Node.js version
# ---------------------------------------------------------------------------

def get_node_version(root):
    header = (root / 'src' / 'node_version.h').read_text(encoding='utf-8')
    major = re.search(r'#define\s+NODE_MAJOR_VERSION\s+(\d+)', header).group(1)
    minor = re.search(r'#define\s+NODE_MINOR_VERSION\s+(\d+)', header).group(1)
    patch = re.search(r'#define\s+NODE_PATCH_VERSION\s+(\d+)', header).group(1)
    return f'{major}.{minor}.{patch}'


# ---------------------------------------------------------------------------
# Version extractor helpers
# ---------------------------------------------------------------------------

def _safe_read(path):
    """Return file text or None on error, with a stderr warning."""
    try:
        return Path(path).read_text(encoding='utf-8', errors='replace')
    except OSError as exc:
        print(f'warning: cannot read {path}: {exc}', file=sys.stderr)
        return None


def version_from_c_macros(path, major_macro, minor_macro, patch_macro):
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    def _extract(macro):
        m = re.search(rf'#\s*define\s+{re.escape(macro)}\s+(\d+)', text)
        return m.group(1) if m else None
    major = _extract(major_macro)
    minor = _extract(minor_macro)
    patch = _extract(patch_macro)
    if major is None or minor is None or patch is None:
        print(f'warning: could not extract version macros from {path}', file=sys.stderr)
        return 'unknown'
    return f'{major}.{minor}.{patch}'


def version_from_string_macro(path, macro_name):
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    m = re.search(rf'#\s*define\s+{re.escape(macro_name)}\s+"([^"]+)"', text)
    if not m:
        print(f'warning: macro {macro_name} not found in {path}', file=sys.stderr)
        return 'unknown'
    return m.group(1)


def version_from_cmake(path):
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    # Matches: project(name VERSION "1.2.3") or project(name VERSION 1.2.3)
    m = re.search(r'[Pp][Rr][Oo][Jj][Ee][Cc][Tt]\s*\([^)]*?VERSION\s+"?([0-9][^\s")]+)"?', text)
    if not m:
        # Try set(PROJECT_VERSION ...) form
        m = re.search(r'[Ss][Ee][Tt]\s*\(\s*PROJECT_VERSION\s+"?([0-9][^\s")]+)"?', text)
    if not m:
        print(f'warning: could not extract VERSION from {path}', file=sys.stderr)
        return 'unknown'
    return m.group(1)


def version_from_package_json(path):
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    try:
        return json.loads(text)['version']
    except (KeyError, json.JSONDecodeError) as exc:
        print(f'warning: could not read version from {path}: {exc}', file=sys.stderr)
        return 'unknown'


def version_from_plain_file(path):
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    return text.strip()


def version_from_icu_dep(path):
    """Parse tools/icu/current_ver.dep (JSON array) for the ICU version."""
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    try:
        data = json.loads(text)
        url = data[0]['url']
    except (json.JSONDecodeError, KeyError, IndexError, TypeError) as exc:
        print(f'warning: could not parse {path}: {exc}', file=sys.stderr)
        return 'unknown'
    m = re.search(r'icu4c-([^-]+(?:\.\d+)*)-sources\.tgz', url)
    if not m:
        print(f'warning: ICU version not found in URL: {url}', file=sys.stderr)
        return 'unknown'
    return m.group(1)


def version_from_readme_node(path, field):
    """Extract a named field from a README.node-style metadata file."""
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    pattern = rf'^{re.escape(field)}:\s*(.+)$'
    m = re.search(pattern, text, re.MULTILINE)
    if not m:
        print(f'warning: field "{field}" not found in {path}', file=sys.stderr)
        return 'unknown'
    return m.group(1).strip()


def version_from_cargo_toml(path):
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    m = re.search(r'^version\s*=\s*"([^"]+)"', text, re.MULTILINE)
    if not m:
        print(f'warning: version not found in {path}', file=sys.stderr)
        return 'unknown'
    return m.group(1)


def version_from_openssl(root):
    """OpenSSL's version header is arch-specific; glob for the first match."""
    pattern = str(root / 'deps' / 'openssl' / 'config' / 'archs' /
                  '*/asm/include/openssl/opensslv.h')
    matches = sorted(glob.glob(pattern))
    if not matches:
        print(f'warning: no OpenSSL arch-specific opensslv.h found', file=sys.stderr)
        return 'unknown'
    return version_from_string_macro(matches[0], 'OPENSSL_VERSION_STR')


def _version_from_texi(path):
    """Extract @set VERSION X.Y.Z from a Texinfo version file."""
    text = _safe_read(path)
    if text is None:
        return 'unknown'
    m = re.search(r'^@set\s+VERSION\s+(.+)$', text, re.MULTILINE)
    if not m:
        print(f'warning: VERSION not found in {path}', file=sys.stderr)
        return 'unknown'
    return m.group(1).strip()


# ---------------------------------------------------------------------------
# DEP_CATALOG
# Each entry describes one bundled dependency.
#
# Fields:
#   name              canonical dep name
#   description       short human-readable description
#   purl_type         PURL type: "generic", "github", "npm", "cargo"
#   purl_namespace    PURL namespace (org or scope)
#   purl_name         PURL package name
#   component_type    CycloneDX component type: "library" | "framework"
#   default_scope     "required" | "optional" | "excluded"
#   config_use_flag   variables key that must be "true" for inclusion (None = always on)
#   config_shared_flag  if this variables key is "true", mark scope as "excluded"
#   extra_conditions  list of (key, bad_value) pairs that also cause scope="excluded"
#   version_fn        callable(root: Path) -> str
#
# When adding a new bundled C/C++ dependency to deps/:
#   1. Add an entry to this list following the pattern below.
#   2. Set config_use_flag / config_shared_flag to match the variable names
#      declared in node.gyp's 'variables' block.
#   3. Set version_fn to the appropriate extractor for that dep's version file.
#
# JS lib files (lib/*.js) do NOT need an entry here — they are captured
# automatically via node_library_files in config.gypi.
# ---------------------------------------------------------------------------

DEP_CATALOG = [
    # --- Runtime C/C++ deps (config-flag controlled) ---
    {
        'name': 'openssl',
        'description': 'TLS/SSL and general-purpose cryptography library',
        'purl_type': 'github',
        'purl_namespace': 'openssl',
        'purl_name': 'openssl',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': 'node_use_openssl',
        'config_shared_flag': 'node_shared_openssl',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_openssl(root),
    },
    {
        'name': 'zlib',
        'description': 'General-purpose lossless data compression library',
        'purl_type': 'github',
        'purl_namespace': 'madler',
        'purl_name': 'zlib',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_zlib',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_cmake(
            root / 'deps' / 'zlib' / 'CMakeLists.txt'),
    },
    {
        'name': 'libuv',
        'description': 'Multi-platform support library with a focus on asynchronous I/O',
        'purl_type': 'github',
        'purl_namespace': 'libuv',
        'purl_name': 'libuv',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_libuv',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_c_macros(
            root / 'deps' / 'uv' / 'include' / 'uv' / 'version.h',
            'UV_VERSION_MAJOR', 'UV_VERSION_MINOR', 'UV_VERSION_PATCH'),
    },
    {
        'name': 'v8',
        'description': "Google's open source JavaScript and WebAssembly engine",
        'purl_type': 'github',
        'purl_namespace': 'v8',
        'purl_name': 'v8',
        'component_type': 'framework',
        'default_scope': 'required',
        'config_use_flag': 'node_use_bundled_v8',
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_c_macros(
            root / 'deps' / 'v8' / 'include' / 'v8-version.h',
            'V8_MAJOR_VERSION', 'V8_MINOR_VERSION', 'V8_BUILD_NUMBER'),
    },
    {
        'name': 'brotli',
        'description': 'Brotli compression algorithm',
        'purl_type': 'github',
        'purl_namespace': 'google',
        'purl_name': 'brotli',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_brotli',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_c_macros(
            root / 'deps' / 'brotli' / 'c' / 'common' / 'version.h',
            'BROTLI_VERSION_MAJOR', 'BROTLI_VERSION_MINOR', 'BROTLI_VERSION_PATCH'),
    },
    {
        'name': 'c-ares',
        'description': 'Asynchronous DNS resolver library',
        'purl_type': 'github',
        'purl_namespace': 'c-ares',
        'purl_name': 'c-ares',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_cares',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_cmake(
            root / 'deps' / 'cares' / 'CMakeLists.txt'),
    },
    {
        'name': 'llhttp',
        'description': 'HTTP/1.1 request and response parser',
        'purl_type': 'github',
        'purl_namespace': 'nodejs',
        'purl_name': 'llhttp',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_http_parser',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_cmake(
            root / 'deps' / 'llhttp' / 'CMakeLists.txt'),
    },
    {
        'name': 'nghttp2',
        'description': 'HTTP/2 C library',
        'purl_type': 'github',
        'purl_namespace': 'nghttp2',
        'purl_name': 'nghttp2',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_nghttp2',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'nghttp2' / 'lib' / 'includes' / 'nghttp2' / 'nghttp2ver.h',
            'NGHTTP2_VERSION'),
    },
    {
        'name': 'ngtcp2',
        'description': 'QUIC protocol implementation (ngtcp2)',
        'purl_type': 'github',
        'purl_namespace': 'ngtcp2',
        'purl_name': 'ngtcp2',
        'component_type': 'library',
        'default_scope': 'optional',
        'config_use_flag': 'node_use_quic',
        'config_shared_flag': 'node_shared_ngtcp2',
        # Also excluded when OpenSSL is system-provided
        'extra_conditions': [('node_shared_openssl', 'true')],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'ngtcp2' / 'ngtcp2' / 'lib' / 'includes' / 'ngtcp2' / 'version.h',
            'NGTCP2_VERSION'),
    },
    {
        'name': 'nghttp3',
        'description': 'HTTP/3 library (embedded in ngtcp2)',
        'purl_type': 'github',
        'purl_namespace': 'ngtcp2',
        'purl_name': 'nghttp3',
        'component_type': 'library',
        'default_scope': 'optional',
        'config_use_flag': 'node_use_quic',
        'config_shared_flag': 'node_shared_nghttp3',
        'extra_conditions': [('node_shared_openssl', 'true')],
        # nghttp3 is embedded inside deps/ngtcp2 with no separate version file
        'version_fn': lambda root: 'unknown',
    },
    {
        'name': 'ada',
        'description': 'Fast WHATWG-compliant URL parser',
        'purl_type': 'github',
        'purl_namespace': 'ada-url',
        'purl_name': 'ada',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_ada',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'ada' / 'ada.h', 'ADA_VERSION'),
    },
    {
        'name': 'simdjson',
        'description': 'Parsing gigabytes of JSON per second',
        'purl_type': 'github',
        'purl_namespace': 'simdjson',
        'purl_name': 'simdjson',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_simdjson',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'simdjson' / 'simdjson.h', 'SIMDJSON_VERSION'),
    },
    {
        'name': 'sqlite',
        'description': 'Self-contained SQL database engine',
        'purl_type': 'github',
        'purl_namespace': 'sqlite',
        'purl_name': 'sqlite',
        'component_type': 'library',
        'default_scope': 'optional',
        'config_use_flag': 'node_use_sqlite',
        'config_shared_flag': 'node_shared_sqlite',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'sqlite' / 'sqlite3.h', 'SQLITE_VERSION'),
    },
    {
        'name': 'uvwasi',
        'description': 'WASI syscall API built atop libuv',
        'purl_type': 'github',
        'purl_namespace': 'nodejs',
        'purl_name': 'uvwasi',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_uvwasi',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_c_macros(
            root / 'deps' / 'uvwasi' / 'include' / 'uvwasi.h',
            'UVWASI_VERSION_MAJOR', 'UVWASI_VERSION_MINOR', 'UVWASI_VERSION_PATCH'),
    },
    {
        'name': 'zstd',
        'description': 'Fast lossless compression algorithm',
        'purl_type': 'github',
        'purl_namespace': 'facebook',
        'purl_name': 'zstd',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_zstd',
        'extra_conditions': [],
        # Third macro is RELEASE not PATCH
        'version_fn': lambda root: version_from_c_macros(
            root / 'deps' / 'zstd' / 'lib' / 'zstd.h',
            'ZSTD_VERSION_MAJOR', 'ZSTD_VERSION_MINOR', 'ZSTD_VERSION_RELEASE'),
    },
    {
        'name': 'hdr_histogram',
        'description': 'High Dynamic Range (HDR) Histogram',
        'purl_type': 'github',
        'purl_namespace': 'HdrHistogram',
        'purl_name': 'HdrHistogram_c',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_hdr_histogram',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'histogram' / 'include' / 'hdr' / 'hdr_histogram_version.h',
            'HDR_HISTOGRAM_VERSION'),
    },
    {
        'name': 'perfetto',
        'description': 'Production-grade open-source stack for performance instrumentation',
        'purl_type': 'github',
        'purl_namespace': 'google',
        'purl_name': 'perfetto',
        'component_type': 'library',
        'default_scope': 'optional',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_perfetto',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_plain_file(
            root / 'deps' / 'perfetto' / 'VERSION'),
    },
    {
        'name': 'LIEF',
        'description': 'Library to Instrument Executable Formats',
        'purl_type': 'github',
        'purl_namespace': 'lief-project',
        'purl_name': 'LIEF',
        'component_type': 'library',
        'default_scope': 'optional',
        'config_use_flag': 'node_use_lief',
        'config_shared_flag': 'node_shared_lief',
        'extra_conditions': [],
        # Use MAJOR/MINOR/PATCH — LIEF_VERSION has a trailing '-'
        'version_fn': lambda root: version_from_c_macros(
            root / 'deps' / 'LIEF' / 'include' / 'LIEF' / 'version.h',
            'LIEF_VERSION_MAJOR', 'LIEF_VERSION_MINOR', 'LIEF_VERSION_PATCH'),
    },
    {
        'name': 'libffi',
        'description': 'Portable foreign-function interface library',
        'purl_type': 'github',
        'purl_namespace': 'libffi',
        'purl_name': 'libffi',
        'component_type': 'library',
        'default_scope': 'optional',
        'config_use_flag': 'node_use_ffi',
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: _version_from_texi(
            root / 'deps' / 'libffi' / 'doc' / 'version.texi'),
    },
    {
        'name': 'merve',
        'description': 'Simple string interning library used by Node.js',
        'purl_type': 'github',
        'purl_namespace': 'anonrig',
        'purl_name': 'merve',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_merve',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'merve' / 'merve.h', 'MERVE_VERSION'),
    },
    {
        'name': 'nbytes',
        'description': 'Byte manipulation utilities for Node.js',
        'purl_type': 'github',
        'purl_namespace': 'nodejs',
        'purl_name': 'nbytes',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': 'node_shared_nbytes',
        'extra_conditions': [],
        'version_fn': lambda root: version_from_string_macro(
            root / 'deps' / 'nbytes' / 'include' / 'nbytes.h', 'NBYTES_VERSION'),
    },
    {
        'name': 'ncrypto',
        'description': 'Internal Node.js cryptography helper library',
        'purl_type': 'generic',
        'purl_namespace': 'nodejs',
        'purl_name': 'ncrypto',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: 'node-internal',
    },
    {
        'name': 'inspector_protocol',
        'description': 'Chrome DevTools Protocol (inspector) implementation',
        'purl_type': 'generic',
        'purl_namespace': 'chromium',
        'purl_name': 'inspector_protocol',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_readme_node(
            root / 'deps' / 'inspector_protocol' / 'README.node', 'Revision'),
    },
    # --- Always-bundled JS/TS/Rust deps ---
    {
        'name': 'icu',
        'description': 'International Components for Unicode',
        'purl_type': 'github',
        'purl_namespace': 'unicode-org',
        'purl_name': 'icu',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_icu_dep(
            root / 'tools' / 'icu' / 'current_ver.dep'),
    },
    {
        'name': 'npm',
        'description': 'JavaScript package manager',
        'purl_type': 'npm',
        'purl_namespace': '',
        'purl_name': 'npm',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_package_json(
            root / 'deps' / 'npm' / 'package.json'),
    },
    {
        'name': 'undici',
        'description': 'HTTP/1.1 client for Node.js',
        'purl_type': 'npm',
        'purl_namespace': '',
        'purl_name': 'undici',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        # Note: version file is under src/, not at the top level of deps/undici
        'version_fn': lambda root: version_from_package_json(
            root / 'deps' / 'undici' / 'src' / 'package.json'),
    },
    {
        'name': 'acorn',
        'description': 'JavaScript AST parser (includes acorn-walk)',
        'purl_type': 'npm',
        'purl_namespace': '',
        'purl_name': 'acorn',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_package_json(
            root / 'deps' / 'acorn' / 'acorn' / 'package.json'),
    },
    {
        'name': 'amaro',
        'description': 'TypeScript type stripper for Node.js',
        'purl_type': 'npm',
        'purl_namespace': '',
        'purl_name': 'amaro',
        'component_type': 'library',
        'default_scope': 'optional',
        'config_use_flag': 'node_use_amaro',
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_package_json(
            root / 'deps' / 'amaro' / 'package.json'),
    },
    {
        'name': 'corepack',
        'description': 'Package manager version management shim',
        'purl_type': 'npm',
        'purl_namespace': '',
        'purl_name': 'corepack',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_package_json(
            root / 'deps' / 'corepack' / 'package.json'),
    },
    {
        'name': 'v8-crates',
        'description': 'Rust bindings used by the V8 build',
        'purl_type': 'cargo',
        'purl_namespace': 'nodejs',
        'purl_name': 'v8-crates',
        'component_type': 'library',
        'default_scope': 'required',
        'config_use_flag': 'node_use_bundled_v8',
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_cargo_toml(
            root / 'deps' / 'crates' / 'Cargo.toml'),
    },
    # --- Dev/test-only deps (scope: excluded) ---
    {
        'name': 'googletest',
        'description': 'Google C++ testing and mocking framework (test-only)',
        'purl_type': 'github',
        'purl_namespace': 'google',
        'purl_name': 'googletest',
        'component_type': 'library',
        'default_scope': 'excluded',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: 'unknown',
    },
    {
        'name': 'postject',
        'description': 'Utility to inject arbitrary read-only resources into executables (build tool)',
        'purl_type': 'npm',
        'purl_namespace': '',
        'purl_name': 'postject',
        'component_type': 'library',
        'default_scope': 'excluded',
        'config_use_flag': None,
        'config_shared_flag': None,
        'extra_conditions': [],
        'version_fn': lambda root: version_from_package_json(
            root / 'test' / 'fixtures' / 'postject-copy' / 'node_modules' /
            'postject' / 'package.json'),
    },
]


# ---------------------------------------------------------------------------
# Scope resolution
# ---------------------------------------------------------------------------

def _resolve_scope(dep, config_vars):
    """Return the effective CycloneDX scope for a dep given the build config."""
    if dep['default_scope'] == 'excluded':
        return 'excluded'

    use_flag = dep['config_use_flag']
    shared_flag = dep['config_shared_flag']

    # If the feature is explicitly disabled, mark excluded
    if use_flag and config_vars.get(use_flag, 'true') == 'false':
        return 'excluded'

    # If satisfied by the system shared library, mark excluded
    if shared_flag and config_vars.get(shared_flag, 'false') == 'true':
        return 'excluded'

    # Any additional conditions that, when their "bad value" matches, exclude
    for flag, bad_val in dep.get('extra_conditions', []):
        if config_vars.get(flag, '') == bad_val:
            return 'excluded'

    return dep['default_scope']


# ---------------------------------------------------------------------------
# CycloneDX document assembly
# ---------------------------------------------------------------------------

def _build_purl(dep, version):
    purl_type = dep['purl_type']
    ns = dep['purl_namespace']
    name = dep['purl_name']
    if ns:
        return f'pkg:{purl_type}/{ns}/{name}@{version}'
    return f'pkg:{purl_type}/{name}@{version}'


def build_component(dep, version, scope, bom_ref):
    return {
        'bom-ref': bom_ref,
        'type': dep['component_type'],
        'name': dep['name'],
        'version': version,
        'description': dep['description'],
        'scope': scope,
        'purl': _build_purl(dep, version),
    }


def build_sbom(root_version, components, lib_files):
    serial = f'urn:uuid:{uuid.uuid4()}'
    timestamp = datetime.datetime.now(datetime.timezone.utc).isoformat()

    root_bom_ref = f'nodejs@{root_version}'
    root_component = {
        'bom-ref': root_bom_ref,
        'type': 'framework',
        'name': 'nodejs',
        'version': root_version,
        'description': 'Node.js JavaScript runtime',
        'purl': f'pkg:github/nodejs/node@{root_version}',
    }

    # Only active (non-excluded) components are runtime dependencies of the root
    dep_refs = [c['bom-ref'] for c in components if c['scope'] != 'excluded']

    doc = {
        '$schema': 'http://cyclonedx.org/schema/bom-1.5.schema.json',
        'bomFormat': 'CycloneDX',
        'specVersion': '1.5',
        'serialNumber': serial,
        'version': 1,
        'metadata': {
            'timestamp': timestamp,
            'lifecycles': [{'phase': 'build'}],
            'tools': [
                {
                    'vendor': 'Node.js',
                    'name': 'generate-sbom.py',
                    'version': root_version,
                }
            ],
            'component': root_component,
            'properties': [
                {
                    'name': 'nodejs:bundled-lib-files',
                    'value': json.dumps(lib_files),
                }
            ],
        },
        'components': components,
        'dependencies': [
            {
                'ref': root_bom_ref,
                'dependsOn': dep_refs,
            },
            *[{'ref': c['bom-ref'], 'dependsOn': []} for c in components],
        ],
    }
    return doc


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

def validate_sbom(output_path):
    p = Path(output_path)
    if not p.exists():
        print(f'error: {p} not found. Run `make sbom` first.', file=sys.stderr)
        sys.exit(1)

    try:
        doc = json.loads(p.read_text(encoding='utf-8'))
    except json.JSONDecodeError as exc:
        print(f'error: {p} is not valid JSON: {exc}', file=sys.stderr)
        sys.exit(1)

    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    check(doc.get('bomFormat') == 'CycloneDX',
          f'bomFormat: expected "CycloneDX", got {doc.get("bomFormat")!r}')
    check(doc.get('specVersion') == '1.5',
          f'specVersion: expected "1.5", got {doc.get("specVersion")!r}')
    check(isinstance(doc.get('components'), list) and len(doc['components']) > 0,
          'components: must be a non-empty list')
    check(doc.get('metadata', {}).get('component', {}).get('name') == 'nodejs',
          f'metadata.component.name: expected "nodejs"')

    for comp in doc.get('components', []):
        for field in ('type', 'name', 'version', 'purl'):
            check(field in comp and comp[field] != '',
                  f'component {comp.get("name", "?")!r}: missing or empty "{field}"')

    if failures:
        print(f'SBOM validation FAILED ({len(failures)} issue(s)):', file=sys.stderr)
        for f in failures:
            print(f'  - {f}', file=sys.stderr)
        sys.exit(1)

    print(f'SBOM validation passed: {p}')


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Generate or validate a CycloneDX 1.5 SBOM for the Node.js runtime.')
    parser.add_argument(
        '--output',
        help='Output path for the SBOM JSON '
             '(default: out/Release/node-v{version}-sbom.json)')
    parser.add_argument(
        '--validate', action='store_true',
        help='Validate an existing SBOM instead of generating a new one')
    args = parser.parse_args()

    if args.validate:
        # For --validate the output path must be explicit or use the versioned default
        config_vars = parse_config_gypi(REPO_ROOT / 'config.gypi')
        node_version = get_node_version(REPO_ROOT)
        output_path = Path(args.output) if args.output else \
            REPO_ROOT / 'out' / 'Release' / f'node-v{node_version}-sbom.json'
        validate_sbom(output_path)
        return

    config_vars = parse_config_gypi(REPO_ROOT / 'config.gypi')
    node_version = get_node_version(REPO_ROOT)
    output_path = Path(args.output) if args.output else \
        REPO_ROOT / 'out' / 'Release' / f'node-v{node_version}-sbom.json'
    lib_files = config_vars.get('node_library_files', [])

    components = []
    for dep in DEP_CATALOG:
        scope = _resolve_scope(dep, config_vars)
        try:
            version = dep['version_fn'](REPO_ROOT)
        except Exception as exc:
            if scope != 'excluded':
                print(f'warning: version_fn failed for {dep["name"]}: {exc}', file=sys.stderr)
            version = 'unknown'
        bom_ref = f'{dep["name"]}@{version}'
        components.append(build_component(dep, version, scope, bom_ref))

    doc = build_sbom(node_version, components, lib_files)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(doc, indent=2), encoding='utf-8')
    print(f'SBOM written to {output_path}')


if __name__ == '__main__':
    main()
