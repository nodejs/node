#!/usr/bin/env python3
# Generates a CycloneDX 1.7 Software Bill of Materials (SBOM) describing the
# bundled runtime dependencies of the Node.js binary.
#
# Usage:
#   python tools/sbom/generate_sbom.py [--output PATH]
#   python tools/sbom/generate_sbom.py --validate [--output PATH]
#
# The output defaults to out/Release/node-v{version}-sbom.json. Generating
# requires ./configure to have been run first, as the set of dependencies that
# end up in the binary is read from config.gypi.

import argparse
import datetime
import glob
import json
import os
import platform
import re
import shlex
import shutil
import socket
import subprocess
import sys
import uuid
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent.parent

# Used when a dependency does not record its version anywhere we can read.
UNKNOWN = 'unknown'

# Used when a build-environment fact cannot be determined.
NOT_AVAILABLE = 'N/A'


def parse_config_gypi(path):
  """Read config.gypi and return its `variables` dict."""
  if not path.exists():
    print(f'error: {path} not found.\n'
          'Run ./configure first to generate the build configuration.',
          file=sys.stderr)
    sys.exit(1)
  # config.gypi is JSON apart from a leading "# Do not edit" comment.
  text = path.read_text(encoding='utf-8')
  json_text = '\n'.join(
    line for line in text.splitlines() if not line.startswith('#'))
  return json.loads(json_text).get('variables', {})


def get_node_version(root):
  header = (root / 'src/node_version.h').read_text(encoding='utf-8')
  parts = [re.search(rf'#define\s+NODE_{part}_VERSION\s+(\d+)', header).group(1)
           for part in ('MAJOR', 'MINOR', 'PATCH')]
  return '.'.join(parts)


# ---------------------------------------------------------------------------
# Build environment
#
# Facts about the machine and toolchain that produced the binary. Like the
# version extractors below, they never raise: anything that cannot be
# determined is recorded as NOT_AVAILABLE so one failed probe never prevents
# the SBOM from being generated.
# ---------------------------------------------------------------------------

def _run(cmd_args, timeout=10):
  """Run `cmd_args` (a list, never a shell string) and return its stdout.

  Returns None if the command is missing, fails, times out or prints nothing.
  """
  try:
    result = subprocess.run(cmd_args, capture_output=True, text=True,
                            timeout=timeout, check=False)
  except (OSError, ValueError, subprocess.SubprocessError):
    return None
  if result.returncode != 0:
    return None
  return result.stdout.strip() or None


def get_compiler_version(env_var, default):
  """Report the tool named by `$env_var` (or `default`) and its version.

  Returns {'command': str, 'output': str} where `output` is the first line of
  `<tool> --version`, or NOT_AVAILABLE if the tool cannot be run. The variable
  is split with shlex because it may hold a wrapper or flags (`ccache gcc`).
  """
  command = os.environ.get(env_var) or default
  try:
    args = shlex.split(command)
  except ValueError:
    return {'command': command, 'output': NOT_AVAILABLE}
  output = _run(args + ['--version']) if args else None
  first_line = output.splitlines()[0].strip() if output else ''
  return {'command': command, 'output': first_line or NOT_AVAILABLE}


def _parse_version(output):
  """Pull a dotted version number out of `output`, else return it as is."""
  match = re.search(r'\d+(?:\.\d+)+', output)
  return match.group(0) if match else output


def get_git_sha(root):
  """Return (alg, digest) for the commit `root` is checked out at.

  Returns (NOT_AVAILABLE, NOT_AVAILABLE) when the repo is a source tarball
  or git is not installed.  SHA-1 repositories produce a 40-hex-digit digest
  (alg 'SHA-1'); SHA-256 repositories produce a 64-hex-digit digest
  (alg 'SHA-256').
  """
  output = _run(['git', '-C', str(root), 'rev-parse', 'HEAD'])
  if output:
    if re.fullmatch(r'[0-9a-f]{40}', output):
      return 'SHA-1', output
    if re.fullmatch(r'[0-9a-f]{64}', output):
      return 'SHA-256', output
  return NOT_AVAILABLE, NOT_AVAILABLE


def get_v8_embedder_string(root):
  """Return V8's embedder string from common.gypi, or NOT_AVAILABLE.

  It is a gyp variable that does not reach config.gypi, so it is read from
  the source. It identifies the set of V8 patches applied on top of upstream.
  """
  value = _search(Path(root) / 'common.gypi',
                  r"'v8_embedder_string':\s*'([^']*)'")
  return NOT_AVAILABLE if value in (UNKNOWN, '') else value


def get_builder_name():
  """Return the hostname of the machine doing the build, or NOT_AVAILABLE."""
  try:
    return socket.gethostname().strip() or NOT_AVAILABLE
  except OSError:
    return NOT_AVAILABLE


def get_configure_command(root):
  """Return the ./configure command that configured the tree, or NOT_AVAILABLE.

  configure.py writes config.status as a shell script ending in
  `exec ./configure <shlex-quoted argv>`. A quoted argument may contain a
  newline, so the whole text after `exec ` is tokenized, not just one line.
  Only argv is recorded there, not environment variables such as CFLAGS.
  """
  text = _read_text(Path(root) / 'config.status')
  if text is None:
    return NOT_AVAILABLE
  match = re.search(r'^exec (.*)\Z', text, re.MULTILINE | re.DOTALL)
  if match is None:
    return NOT_AVAILABLE
  try:
    args = shlex.split(match.group(1))
  except ValueError:
    return NOT_AVAILABLE
  if not args or args[0] != './configure':
    return NOT_AVAILABLE
  return shlex.join(args)


def get_glibc_version():
  """Return the build host's glibc version string, or NOT_AVAILABLE.

  Uses platform.libc_ver() (pure Python, no subprocess). Only meaningful on
  Linux; returns NOT_AVAILABLE on all other platforms and for musl or other
  non-glibc libc implementations.

  Note: this is the host's glibc version. Because Node.js does not build
  against a sysroot targeting an older glibc, this version becomes the
  binary's minimum glibc requirement at run time.
  """
  if sys.platform != 'linux':
    return NOT_AVAILABLE
  lib, version = platform.libc_ver()
  if lib == 'glibc' and version:
    return version
  return NOT_AVAILABLE


def get_compiler_package_version(binary_path):
  """Return the distro package version that owns `binary_path`, or NOT_AVAILABLE.

  Tries dpkg (Debian/Ubuntu) then rpm (RHEL/Fedora). `binary_path` must be an
  absolute path string as returned by shutil.which(). Only runs on Linux.
  """
  if sys.platform != 'linux' or not binary_path:
    return NOT_AVAILABLE

  # --- dpkg (Debian / Ubuntu) ---
  # dpkg -S resolves the package name from the file path.
  dpkg_s = _run(['dpkg', '-S', binary_path])
  if dpkg_s:
    # Output is "pkgname: /path/to/binary"; extract the package name.
    pkg_name = dpkg_s.split(':')[0].strip()
    if pkg_name:
      version = _run(['dpkg-query', '-W', '-f=${Version}', pkg_name])
      if version:
        return version

  # --- rpm (RHEL / Fedora / SUSE) ---
  rpm_out = _run(['rpm', '-qf', '--queryformat', '%{VERSION}-%{RELEASE}',
                  binary_path])
  if rpm_out:
    return rpm_out

  return NOT_AVAILABLE


def collect_build_environment(root, config_vars):
  """Return a dict of build-environment facts for build_sbom().

  Each fact is a string, with NOT_AVAILABLE when it cannot be determined.
  """
  # Same defaults as configure.py, so an unset CC/CXX reports what configure
  # actually used.
  if sys.platform == 'darwin':
    default_cc, default_cxx = 'cc', 'c++'
  elif sys.platform == 'win32':
    default_cc, default_cxx = 'clang', 'clang'
  else:
    default_cc, default_cxx = 'gcc', 'g++'

  compilers = []
  for name, env_var, default in (('cc', 'CC', default_cc),
                                 ('cxx', 'CXX', default_cxx),
                                 ('rustc', 'RUSTC', 'rustc'),
                                 ('cargo', 'CARGO', 'cargo')):
    info = get_compiler_version(env_var, default)
    # Resolve the actual binary path for the package-version probe. Only the
    # first token is the executable (shlex.split handles wrappers like ccache).
    try:
      binary = shlex.split(info['command'])[0] if info['command'] else None
    except ValueError:
      binary = None
    info['binary_path'] = shutil.which(binary) if binary else None
    compilers.append({'name': name, **info})

  git_alg, git_sha = get_git_sha(root)
  return {
    'compilers': compilers,
    'glibc_version': get_glibc_version(),
    'git_alg': git_alg,
    'git_sha': git_sha,
    'v8_embedder_string': get_v8_embedder_string(root),
    'builder': get_builder_name(),
    'configure_command': get_configure_command(root),
  }


# ---------------------------------------------------------------------------
# Version extractors
#
# Dependencies record their version in whatever format upstream uses, so each
# one needs an extractor that knows where to look. They never raise: a
# dependency we cannot read a version for is reported as UNKNOWN with a warning
# rather than failing the whole document.
# ---------------------------------------------------------------------------

def _read_text(path):
  """Return the contents of `path`, or None with a warning if unreadable."""
  try:
    return Path(path).read_text(encoding='utf-8', errors='replace')
  except OSError as exc:
    print(f'warning: cannot read {path}: {exc}', file=sys.stderr)
    return None


def _search(path, pattern, flags=0):
  """Return the first capture group of `pattern` in `path`, or UNKNOWN."""
  text = _read_text(path)
  if text is None:
    return UNKNOWN
  match = re.search(pattern, text, flags)
  if match is None:
    print(f'warning: no match for {pattern!r} in {path}', file=sys.stderr)
    return UNKNOWN
  return match.group(1).strip()


def version_from_c_macros(path, *macros):
  """Join integer #define values, e.g. FOO_MAJOR, FOO_MINOR and FOO_PATCH."""
  parts = [_search(path, rf'#\s*define\s+{macro}\s+(\d+)') for macro in macros]
  return UNKNOWN if UNKNOWN in parts else '.'.join(parts)


def version_from_string_macro(path, macro):
  """Read a string #define, e.g. #define FOO_VERSION "1.2.3"."""
  return _search(path, rf'#\s*define\s+{macro}\s+"([^"]+)"')


def version_from_cmake(path):
  """Read the VERSION argument of a CMake project() declaration."""
  return _search(path, r'project\s*\([^)]*?VERSION\s+"?([0-9][^\s")]+)',
                 re.IGNORECASE)


def version_from_package_json(path):
  text = _read_text(path)
  if text is None:
    return UNKNOWN
  try:
    return json.loads(text)['version']
  except (KeyError, json.JSONDecodeError) as exc:
    print(f'warning: cannot read version from {path}: {exc}', file=sys.stderr)
    return UNKNOWN


def version_from_plain_file(path):
  """Read a file whose entire contents are the version."""
  text = _read_text(path)
  return UNKNOWN if text is None else text.strip()


def version_from_cargo_toml(path):
  """Read the first `version = "1.2.3"` key, i.e. the one in [package]."""
  return _search(path, r'^version\s*=\s*"([^"]+)"', re.MULTILINE)


def version_from_texi(path):
  """Read `@set VERSION 1.2.3` from a Texinfo version file."""
  return _search(path, r'^@set\s+VERSION\s+(.+)$', re.MULTILINE)


def version_from_readme_node(path, field):
  """Read a `Field: value` line from a deps/*/README.node metadata file."""
  return _search(path, rf'^{field}:\s*(.+)$', re.MULTILINE)


def version_from_icu_dep(path):
  """Read the ICU version from the source URL in tools/icu/current_ver.dep."""
  return _search(path, r'icu4c-([^-]+)-sources\.tgz')


def version_from_openssl(root):
  """OpenSSL's generated version header is per-architecture; any copy will do."""
  matches = sorted(glob.glob(str(
    root / 'deps/openssl/config/archs/*/asm/include/openssl/opensslv.h')))
  if not matches:
    print('warning: no architecture-specific opensslv.h found', file=sys.stderr)
    return UNKNOWN
  return version_from_string_macro(matches[0], 'OPENSSL_VERSION_STR')


# ---------------------------------------------------------------------------
# DEP_CATALOG
#
# One entry per bundled dependency:
#
#   name            canonical dependency name
#   description     short human-readable description
#   purl_base       package URL, without the "@<version>" suffix appended here
#   version_fn      callable(root: Path) -> version string
#   component_type  CycloneDX component type, 'library' (default) or 'framework'
#   scope           CycloneDX scope when the dependency is part of the build:
#                   'required' (default), 'optional', or 'excluded' for
#                   dependencies that are never linked into the binary
#   use_flag        config.gypi variable that must not be 'false'
#   shared_flag     config.gypi variable that is 'true' when a system library
#                   is used instead of the bundled copy
#   excluded_when   (variable, value) pairs that also exclude the dependency
#
# Only name, description, purl_base and version_fn are required; the rest
# default as described above.
#
# When adding a bundled dependency to deps/, add an entry here and point
# version_fn at whichever file in the dependency records its version. Set
# use_flag and shared_flag to match the variable names declared in the
# 'variables' block of node.gyp.
#
# JavaScript files under lib/ do not need an entry: they are listed
# automatically from node_library_files in config.gypi.
# ---------------------------------------------------------------------------

DEP_CATALOG = [
  # --- Runtime C/C++ dependencies ---
  {
    'name': 'openssl',
    'description': 'TLS/SSL and general-purpose cryptography library',
    'purl_base': 'pkg:github/openssl/openssl',
    'shared_flag': 'node_shared_openssl',
    'use_flag': 'node_use_openssl',
    'version_fn': version_from_openssl,
  },
  {
    'name': 'zlib',
    'description': 'General-purpose lossless data compression library',
    'purl_base': 'pkg:github/madler/zlib',
    'shared_flag': 'node_shared_zlib',
    'version_fn': lambda root: version_from_cmake(
      root / 'deps/zlib/CMakeLists.txt'),
  },
  {
    'name': 'libuv',
    'description': 'Multi-platform support library with a focus on '
                   'asynchronous I/O',
    'purl_base': 'pkg:github/libuv/libuv',
    'shared_flag': 'node_shared_libuv',
    'version_fn': lambda root: version_from_c_macros(
      root / 'deps/uv/include/uv/version.h',
      'UV_VERSION_MAJOR', 'UV_VERSION_MINOR', 'UV_VERSION_PATCH'),
  },
  {
    'name': 'v8',
    'description': "Google's open source JavaScript and WebAssembly engine",
    'purl_base': 'pkg:github/v8/v8',
    'component_type': 'framework',
    'use_flag': 'node_use_bundled_v8',
    'version_fn': lambda root: version_from_c_macros(
      root / 'deps/v8/include/v8-version.h',
      'V8_MAJOR_VERSION', 'V8_MINOR_VERSION', 'V8_BUILD_NUMBER'),
  },
  {
    'name': 'brotli',
    'description': 'Brotli compression algorithm',
    'purl_base': 'pkg:github/google/brotli',
    'shared_flag': 'node_shared_brotli',
    'version_fn': lambda root: version_from_c_macros(
      root / 'deps/brotli/c/common/version.h',
      'BROTLI_VERSION_MAJOR', 'BROTLI_VERSION_MINOR', 'BROTLI_VERSION_PATCH'),
  },
  {
    'name': 'c-ares',
    'description': 'Asynchronous DNS resolver library',
    'purl_base': 'pkg:github/c-ares/c-ares',
    'shared_flag': 'node_shared_cares',
    'version_fn': lambda root: version_from_cmake(
      root / 'deps/cares/CMakeLists.txt'),
  },
  {
    'name': 'llhttp',
    'description': 'HTTP/1.1 request and response parser',
    'purl_base': 'pkg:github/nodejs/llhttp',
    'shared_flag': 'node_shared_http_parser',
    'version_fn': lambda root: version_from_cmake(
      root / 'deps/llhttp/CMakeLists.txt'),
  },
  {
    'name': 'nghttp2',
    'description': 'HTTP/2 C library',
    'purl_base': 'pkg:github/nghttp2/nghttp2',
    'shared_flag': 'node_shared_nghttp2',
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/nghttp2/lib/includes/nghttp2/nghttp2ver.h',
      'NGHTTP2_VERSION'),
  },
  {
    'name': 'ngtcp2',
    'description': 'QUIC protocol implementation',
    'purl_base': 'pkg:github/ngtcp2/ngtcp2',
    'scope': 'optional',
    'use_flag': 'node_use_quic',
    'shared_flag': 'node_shared_ngtcp2',
    # QUIC is only built against the bundled OpenSSL.
    'excluded_when': [('node_shared_openssl', 'true')],
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/ngtcp2/ngtcp2/lib/includes/ngtcp2/version.h',
      'NGTCP2_VERSION'),
  },
  {
    'name': 'nghttp3',
    'description': 'HTTP/3 library, bundled inside deps/ngtcp2',
    'purl_base': 'pkg:github/ngtcp2/nghttp3',
    'scope': 'optional',
    'use_flag': 'node_use_quic',
    'shared_flag': 'node_shared_nghttp3',
    'excluded_when': [('node_shared_openssl', 'true')],
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/ngtcp2/nghttp3/lib/includes/nghttp3/version.h',
      'NGHTTP3_VERSION'),
  },
  {
    'name': 'ada',
    'description': 'Fast WHATWG-compliant URL parser',
    'purl_base': 'pkg:github/ada-url/ada',
    'shared_flag': 'node_shared_ada',
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/ada/ada.h', 'ADA_VERSION'),
  },
  {
    'name': 'simdjson',
    'description': 'SIMD-accelerated JSON parser',
    'purl_base': 'pkg:github/simdjson/simdjson',
    'shared_flag': 'node_shared_simdjson',
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/simdjson/simdjson.h', 'SIMDJSON_VERSION'),
  },
  {
    'name': 'sqlite',
    'description': 'Self-contained SQL database engine',
    'purl_base': 'pkg:github/sqlite/sqlite',
    'scope': 'optional',
    'use_flag': 'node_use_sqlite',
    'shared_flag': 'node_shared_sqlite',
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/sqlite/sqlite3.h', 'SQLITE_VERSION'),
  },
  {
    'name': 'uvwasi',
    'description': 'WASI syscall API built atop libuv',
    'purl_base': 'pkg:github/nodejs/uvwasi',
    'shared_flag': 'node_shared_uvwasi',
    'version_fn': lambda root: version_from_c_macros(
      root / 'deps/uvwasi/include/uvwasi.h',
      'UVWASI_VERSION_MAJOR', 'UVWASI_VERSION_MINOR', 'UVWASI_VERSION_PATCH'),
  },
  {
    'name': 'zstd',
    'description': 'Fast lossless compression algorithm',
    'purl_base': 'pkg:github/facebook/zstd',
    'shared_flag': 'node_shared_zstd',
    # The third component is RELEASE rather than PATCH.
    'version_fn': lambda root: version_from_c_macros(
      root / 'deps/zstd/lib/zstd.h',
      'ZSTD_VERSION_MAJOR', 'ZSTD_VERSION_MINOR', 'ZSTD_VERSION_RELEASE'),
  },
  {
    'name': 'hdr_histogram',
    'description': 'High Dynamic Range (HDR) Histogram',
    'purl_base': 'pkg:github/HdrHistogram/HdrHistogram_c',
    'shared_flag': 'node_shared_hdr_histogram',
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/histogram/include/hdr/hdr_histogram_version.h',
      'HDR_HISTOGRAM_VERSION'),
  },
  {
    'name': 'perfetto',
    'description': 'Performance instrumentation and tracing SDK',
    'purl_base': 'pkg:github/google/perfetto',
    'scope': 'optional',
    # Included only with --with-perfetto; omitted by default.
    'excluded_when': [('v8_use_perfetto', 0)],
    'shared_flag': 'node_shared_perfetto',
    'version_fn': lambda root: version_from_plain_file(
      root / 'deps/perfetto/VERSION'),
  },
  {
    'name': 'LIEF',
    'description': 'Library to Instrument Executable Formats',
    'purl_base': 'pkg:github/lief-project/LIEF',
    'scope': 'optional',
    'use_flag': 'node_use_lief',
    'shared_flag': 'node_shared_lief',
    # LIEF_VERSION has a trailing '-', so build it from the numeric macros.
    'version_fn': lambda root: version_from_c_macros(
      root / 'deps/LIEF/include/LIEF/version.h',
      'LIEF_VERSION_MAJOR', 'LIEF_VERSION_MINOR', 'LIEF_VERSION_PATCH'),
  },
  {
    'name': 'libffi',
    'description': 'Portable foreign-function interface library',
    'purl_base': 'pkg:github/libffi/libffi',
    'scope': 'optional',
    'use_flag': 'node_use_ffi',
    'shared_flag': 'node_shared_ffi',
    'version_fn': lambda root: version_from_texi(
      root / 'deps/libffi/doc/version.texi'),
  },
  {
    'name': 'merve',
    'description': 'String interning library',
    'purl_base': 'pkg:github/anonrig/merve',
    'shared_flag': 'node_shared_merve',
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/merve/merve.h', 'MERVE_VERSION'),
  },
  {
    'name': 'nbytes',
    'description': 'Byte manipulation utilities for Node.js',
    'purl_base': 'pkg:github/nodejs/nbytes',
    'shared_flag': 'node_shared_nbytes',
    'version_fn': lambda root: version_from_string_macro(
      root / 'deps/nbytes/include/nbytes.h', 'NBYTES_VERSION'),
  },
  {
    'name': 'ncrypto',
    'description': 'Node.js cryptography helper library',
    'purl_base': 'pkg:generic/nodejs/ncrypto',
    # ncrypto is maintained in this repository and released with Node.js.
    'version_fn': get_node_version,
  },
  {
    'name': 'inspector_protocol',
    'description': 'Chrome DevTools Protocol (inspector) implementation',
    'purl_base': 'pkg:generic/chromium/inspector_protocol',
    # Upstream is unversioned, so the vendored revision identifies the copy.
    # Omitted with --without-inspector or --without-ssl.
    'excluded_when': [('v8_enable_inspector', 0)],
    'version_fn': lambda root: version_from_readme_node(
      root / 'deps/inspector_protocol/README.node', 'Revision'),
  },
  # --- ICU, JavaScript and Rust dependencies ---
  {
    'name': 'icu',
    'description': 'International Components for Unicode',
    'purl_base': 'pkg:github/unicode-org/icu',
    # Omitted when built with --with-intl=none.
    'scope': 'optional',
    'excluded_when': [('v8_enable_i18n_support', 0)],
    'version_fn': lambda root: version_from_icu_dep(
      root / 'tools/icu/current_ver.dep'),
  },
  {
    'name': 'npm',
    'description': 'JavaScript package manager',
    'purl_base': 'pkg:npm/npm',
    # Omitted when built with --without-npm.
    'excluded_when': [('node_install_npm', 'false')],
    'version_fn': lambda root: version_from_package_json(
      root / 'deps/npm/package.json'),
  },
  {
    'name': 'undici',
    'description': 'HTTP/1.1 client for Node.js',
    'purl_base': 'pkg:npm/undici',
    # The manifest lives under src/, not at the top level of deps/undici.
    'version_fn': lambda root: version_from_package_json(
      root / 'deps/undici/src/package.json'),
  },
  {
    'name': 'acorn',
    'description': 'JavaScript AST parser, including acorn-walk',
    'purl_base': 'pkg:npm/acorn',
    'version_fn': lambda root: version_from_package_json(
      root / 'deps/acorn/acorn/package.json'),
  },
  {
    'name': 'amaro',
    'description': 'TypeScript type stripper for Node.js',
    'purl_base': 'pkg:npm/amaro',
    'scope': 'optional',
    'use_flag': 'node_use_amaro',
    'version_fn': lambda root: version_from_package_json(
      root / 'deps/amaro/package.json'),
  },
  {
    'name': 'corepack',
    'description': 'Package manager version management shim',
    'purl_base': 'pkg:npm/corepack',
    # Not bundled by default; included only with --with-corepack.
    'scope': 'optional',
    'excluded_when': [('node_install_corepack', 'false')],
    'version_fn': lambda root: version_from_package_json(
      root / 'deps/corepack/package.json'),
  },
  {
    'name': 'v8-crates',
    'description': 'Generated Cargo workspace of the Rust crates vendored '
                   'under deps/crates/vendor for the V8 build',
    # Not published to crates.io, so this is not a pkg:cargo coordinate.
    'purl_base': 'pkg:generic/nodejs/node_crates',
    # Omitted with --v8-disable-temporal-support or when no Rust compiler is
    # present (configure auto-disables Temporal in that case), or when
    # --shared-temporal_capi links against a system-provided library.
    'scope': 'optional',
    'use_flag': 'node_use_bundled_v8',
    'shared_flag': 'node_shared_temporal_capi',
    'excluded_when': [('v8_enable_temporal_support', 0)],
    'version_fn': lambda root: version_from_cargo_toml(
      root / 'deps/crates/Cargo.toml'),
  },
  # --- Dependencies that are never linked into the binary ---
  {
    'name': 'googletest',
    'description': 'Google C++ testing and mocking framework, used by cctest',
    'purl_base': 'pkg:github/google/googletest',
    'scope': 'excluded',
    # The vendored copy carries no version file.
    'version_fn': lambda root: UNKNOWN,
  },
  {
    'name': 'postject',
    'description': 'Injects read-only resources into executables, used to '
                   'build single executable applications',
    'purl_base': 'pkg:npm/postject',
    'scope': 'excluded',
    # deps/postject only holds the C header; the test fixture pins the version
    # of the npm package that builds single executable applications.
    'version_fn': lambda root: version_from_package_json(
      root / 'test/fixtures/postject-copy/node_modules/postject/package.json'),
  },
]


def resolve_scope(dep, config_vars):
  """Return the CycloneDX scope for `dep` under the current build config."""
  scope = dep.get('scope', 'required')
  if scope == 'excluded':
    return scope

  # The feature was turned off at configure time.
  use_flag = dep.get('use_flag')
  if use_flag and config_vars.get(use_flag, 'true') == 'false':
    return 'excluded'

  # A system-wide shared library is used instead of the bundled copy.
  shared_flag = dep.get('shared_flag')
  if shared_flag and config_vars.get(shared_flag, 'false') == 'true':
    return 'excluded'

  for variable, value in dep.get('excluded_when', []):
    if config_vars.get(variable) == value:
      return 'excluded'

  return scope


def build_component(dep, version, scope):
  return {
    'bom-ref': f'{dep["name"]}@{version}',
    'type': dep.get('component_type', 'library'),
    'name': dep['name'],
    'version': version,
    'description': dep['description'],
    'scope': scope,
    'purl': f'{dep["purl_base"]}@{version}',
  }


def build_sbom(node_version, components, lib_files, build_env=None):
  # `build_env` is the dict from collect_build_environment(); fields missing
  # from it are left out of the document.
  build_env = build_env or {}
  root_ref = f'nodejs@{node_version}'
  root_component = {
    'bom-ref': root_ref,
    'type': 'framework',
    'name': 'nodejs',
    'version': node_version,
    'description': 'Node.js JavaScript runtime',
    'purl': f'pkg:generic/nodejs/node@{node_version}',
  }

  properties = [{
    'name': 'nodejs:bundled-lib-files',
    'value': json.dumps(lib_files),
  }]
  # The raw version line is kept as a property so nothing is lost when the
  # parsed component version is imprecise.
  compilers = build_env.get('compilers', [])
  properties.extend({
    'name': f'nodejs:{c["name"]}-version',
    'value': c['output'],
  } for c in compilers)

  git_sha = build_env.get('git_sha')
  git_alg = build_env.get('git_alg')
  if git_sha and git_sha != NOT_AVAILABLE:
    properties.append({'name': 'nodejs:git-sha', 'value': git_sha})
    # Pins the exact source tree, which the purl version alone does not.
    # Uses the hashes array so the algorithm is explicit and machine-readable.
    root_component['externalReferences'] = [{
      'type': 'vcs',
      'url': 'https://github.com/nodejs/node',
      'hashes': [{'alg': git_alg, 'content': git_sha}],
    }]
  v8_embedder_string = build_env.get('v8_embedder_string')
  if v8_embedder_string:
    properties.append({'name': 'nodejs:v8-embedder-string',
                       'value': v8_embedder_string})
  builder = build_env.get('builder')
  if builder:
    properties.append({'name': 'nodejs:builder', 'value': builder})

  sbom = {
    '$schema': 'http://cyclonedx.org/schema/bom-1.7.schema.json',
    'bomFormat': 'CycloneDX',
    'specVersion': '1.7',
    'serialNumber': f'urn:uuid:{uuid.uuid4()}',
    'version': 1,
    'metadata': {
      'timestamp': datetime.datetime.now(datetime.timezone.utc).isoformat(),
      'lifecycles': [{'phase': 'build'}],
      # This script ships with Node.js, so it carries the Node.js version.
      'tools': {
        'components': [{
          'type': 'application',
          'authors': [{'name': 'Node.js'}],
          'name': 'generate_sbom.py',
          'version': node_version,
        }],
      },
      'component': root_component,
      'properties': properties,
    },
    'components': components,
    'dependencies': [
      # All components in the list are linked into the binary; excluded
      # deps are omitted before reaching here. Leaf entries (no
      # sub-dependencies) are intentionally omitted.
      {
        'ref': root_ref,
        'dependsOn': [c['bom-ref'] for c in components],
      },
    ],
  }

  formula = {'bom-ref': 'build-environment'}
  formula_components = []
  if compilers:
    for c in compilers:
      pkg_version = get_compiler_package_version(c.get('binary_path'))
      comp = {
        'bom-ref': f'build-environment:{c["name"]}',
        'type': 'application',
        'name': c['name'],
        'version': (c['output'] if c['output'] == NOT_AVAILABLE
                    else _parse_version(c['output'])),
        'description': f'Build tool from `{c["command"]}`',
      }
      if pkg_version != NOT_AVAILABLE:
        comp['properties'] = [{'name': 'nodejs:package-version',
                                'value': pkg_version}]
      formula_components.append(comp)
  glibc_version = build_env.get('glibc_version')
  if glibc_version and glibc_version != NOT_AVAILABLE:
    formula_components.append({
      'bom-ref': 'build-environment:glibc',
      'type': 'library',
      'name': 'glibc',
      'version': glibc_version,
      'description': 'GNU C Library on the build host',
    })
  if formula_components:
    formula['components'] = formula_components
  configure_command = build_env.get('configure_command')
  if configure_command:
    # Published as-is: it may contain local paths (see the header comment).
    formula['properties'] = [{'name': 'nodejs:configure-command',
                              'value': configure_command}]
  if len(formula) > 1:
    sbom['formulation'] = [formula]
  return sbom


def validate_sbom(path):
  """Check a generated SBOM for the fields consumers rely on."""
  if not path.exists():
    print(f'error: {path} not found. Run `make sbom` first.', file=sys.stderr)
    sys.exit(1)

  try:
    doc = json.loads(path.read_text(encoding='utf-8'))
  except json.JSONDecodeError as exc:
    print(f'error: {path} is not valid JSON: {exc}', file=sys.stderr)
    sys.exit(1)

  failures = []

  def check(condition, message):
    if not condition:
      failures.append(message)

  check(doc.get('bomFormat') == 'CycloneDX',
        f'bomFormat: expected "CycloneDX", got {doc.get("bomFormat")!r}')
  check(doc.get('specVersion') == '1.7',
        f'specVersion: expected "1.7", got {doc.get("specVersion")!r}')
  check(doc.get('components'), 'components: must be a non-empty list')
  check(doc.get('metadata', {}).get('component', {}).get('name') == 'nodejs',
        'metadata.component.name: expected "nodejs"')

  for component in doc.get('components', []):
    for field in ('type', 'name', 'version', 'purl'):
      check(component.get(field),
            f'component {component.get("name", "?")!r}: '
            f'missing or empty "{field}"')

  if failures:
    print(f'SBOM validation FAILED ({len(failures)} issue(s)):', file=sys.stderr)
    for failure in failures:
      print(f'  - {failure}', file=sys.stderr)
    sys.exit(1)

  print(f'SBOM validation passed: {path}')


def generate_sbom(config_vars, node_version):
  components = []
  for dep in DEP_CATALOG:
    scope = resolve_scope(dep, config_vars)
    if scope == 'excluded':
      continue
    try:
      version = dep['version_fn'](REPO_ROOT)
    except Exception as exc:
      # A dependency we cannot read is worth a warning, not a failed build.
      print(f'warning: cannot determine {dep["name"]} version: {exc}',
            file=sys.stderr)
      version = UNKNOWN
    components.append(build_component(dep, version, scope))

  return build_sbom(node_version, components,
                    config_vars.get('node_library_files', []),
                    collect_build_environment(REPO_ROOT, config_vars))


def main():
  parser = argparse.ArgumentParser(
    description='Generate or validate a CycloneDX 1.7 SBOM for Node.js.')
  parser.add_argument('--output', type=Path,
                      help='path of the SBOM JSON file '
                           '(default: out/Release/node-v{version}-sbom.json)')
  parser.add_argument('--validate', action='store_true',
                      help='validate an existing SBOM instead of generating '
                           'a new one')
  args = parser.parse_args()

  node_version = get_node_version(REPO_ROOT)
  output_path = args.output or (
    REPO_ROOT / 'out/Release' / f'node-v{node_version}-sbom.json')

  if args.validate:
    validate_sbom(output_path)
    return

  config_vars = parse_config_gypi(REPO_ROOT / 'config.gypi')
  doc = generate_sbom(config_vars, node_version)
  output_path.parent.mkdir(parents=True, exist_ok=True)
  output_path.write_text(json.dumps(doc, indent=2) + '\n', encoding='utf-8')
  print(f'SBOM written to {output_path}')


if __name__ == '__main__':
  main()
