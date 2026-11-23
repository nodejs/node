"""OpenSSL-related parts of configure.py, including QUIC and DTLS.

tools/nix/v8.nix replaces this file with a no-op stub so that changes to it do
not invalidate the cached V8 build; nothing here may affect how V8 is built.
"""

import os
import re
import shlex
import subprocess
import sys

from packaging.version import Version

# Set by init().
options = CC = b = error = warn = to_utf8 = None
get_version_helper = configure_library = None

def init(configure):
  global options, CC, b, error, warn, to_utf8, get_version_helper, configure_library
  options = configure.options
  CC = configure.CC
  b = configure.b
  error = configure.error
  warn = configure.warn
  to_utf8 = configure.to_utf8
  get_version_helper = configure.get_version_helper
  configure_library = configure.configure_library

def add_options(parser):
  parser.add_argument("--openssl-conf-name",
      action="store",
      dest="openssl_conf_name",
      default='nodejs_conf',
      help="The OpenSSL config appname (config section name) used by Node.js")

  parser.add_argument('--openssl-default-cipher-list',
      action='store',
      dest='openssl_default_cipher_list',
      help='Use the specified cipher list as the default cipher list')

  parser.add_argument("--openssl-no-asm",
      action="store_true",
      dest="openssl_no_asm",
      default=None,
      help="Do not build optimized assembly for OpenSSL")

  parser.add_argument('--openssl-is-fips',
      action='store_true',
      dest='openssl_is_fips',
      default=None,
      help='specifies that the shared OpenSSL library is FIPS capable '
           '(requires --shared-openssl)')

  parser.add_argument('--openssl-use-def-ca-store',
      action='store_true',
      dest='use_openssl_ca_store',
      default=None,
      help='Use OpenSSL supplied CA store instead of compiled-in Mozilla CA copy.')

  parser.add_argument('--openssl-system-ca-path',
      action='store',
      dest='openssl_system_ca_path',
      help='Use the specified path to system CA (PEM format) in addition to '
           'the OpenSSL supplied CA store or compiled-in Mozilla CA copy.')

  parser.add_argument('--experimental-quic',
      action='store_true',
      dest='experimental_quic',
      default=None,
      help='build with experimental QUIC support')

  parser.add_argument('--experimental-dtls',
      action='store_true',
      dest='experimental_dtls',
      default=None,
      help='build with experimental DTLS support')

def add_shared_options(shared_optgroup):
  shared_optgroup.add_argument('--shared-nghttp3',
      action='store_true',
      dest='shared_nghttp3',
      default=None,
      help='link to a shared nghttp3 DLL instead of static linking')

  shared_optgroup.add_argument('--shared-nghttp3-includes',
      action='store',
      dest='shared_nghttp3_includes',
      help='directory containing nghttp3 header files')

  shared_optgroup.add_argument('--shared-nghttp3-libname',
      action='store',
      dest='shared_nghttp3_libname',
      help='alternative lib name to link to [default: %(default)s]')

  shared_optgroup.add_argument('--shared-nghttp3-libpath',
      action='store',
      dest='shared_nghttp3_libpath',
      help='a directory to search for the shared nghttp3 DLLs')

  shared_optgroup.add_argument('--shared-ngtcp2',
      action='store_true',
      dest='shared_ngtcp2',
      default=None,
      help='link to a shared ngtcp2 DLL instead of static linking')

  shared_optgroup.add_argument('--shared-ngtcp2-includes',
      action='store',
      dest='shared_ngtcp2_includes',
      help='directory containing ngtcp2 header files')

  shared_optgroup.add_argument('--shared-ngtcp2-libname',
      action='store',
      dest='shared_ngtcp2_libname',
      help='alternative lib name to link to [default: %(default)s]')

  shared_optgroup.add_argument('--shared-ngtcp2-libpath',
      action='store',
      dest='shared_ngtcp2_libpath',
      help='a directory to search for the shared tcp2 DLLs')

  shared_optgroup.add_argument('--shared-openssl',
      action='store_true',
      dest='shared_openssl',
      default=None,
      help='link to a shared OpenSSl DLL instead of static linking')

  shared_optgroup.add_argument('--shared-openssl-includes',
      action='store',
      dest='shared_openssl_includes',
      help='directory containing OpenSSL header files')

  shared_optgroup.add_argument('--shared-openssl-libname',
      action='store',
      dest='shared_openssl_libname',
      help='alternative lib name to link to [default: %(default)s]')

  shared_optgroup.add_argument('--shared-openssl-libpath',
      action='store',
      dest='shared_openssl_libpath',
      help='a directory to search for the shared OpenSSL DLLs')

def get_nasm_version(asm):
  try:
    proc = subprocess.Popen(shlex.split(asm) + ['-v'],
                            stdin=subprocess.PIPE, stderr=subprocess.PIPE,
                            stdout=subprocess.PIPE)
  except OSError:
    warn('''No acceptable ASM compiler found!
         Please make sure you have installed NASM from https://www.nasm.us
         and refer BUILDING.md.''')
    return '0.0'

  with proc:
    match = re.match(r"NASM version ([2-9]\.[0-9][0-9]+)",
                     to_utf8(proc.communicate()[0]))

  return match.group(1) if match else '0.0'

def get_xcode_version(cc):
  return get_version_helper(
    cc, r"(^Apple (?:clang|LLVM) version) ([0-9]+\.[0-9]+)")

def get_gas_version(cc):
  try:
    custom_env = os.environ.copy()
    custom_env["LC_ALL"] = "C"
    proc = subprocess.Popen(shlex.split(cc) + ['-Wa,-v', '-c', '-o',
                                               '/dev/null', '-x',
                                               'assembler',  '/dev/null'],
                            stdin=subprocess.PIPE, stderr=subprocess.PIPE,
                            stdout=subprocess.PIPE, env=custom_env)
  except OSError:
    error('''No acceptable C compiler found!

       Please make sure you have a C compiler installed on your system and/or
       consider adjusting the CC environment variable if you installed
       it in a non-standard prefix.''')

  with proc:
    gas_ret = to_utf8(proc.communicate()[1])

  match = re.match(r"GNU assembler version ([2-9]\.[0-9]+)", gas_ret)

  if match:
    return match.group(1)

  warn(f'Could not recognize `gas`: {gas_ret}')
  return '0.0'

def get_openssl_macros(o):
  """Extract OpenSSL preprocessor macros from the configured headers."""

  # Use the C compiler to extract preprocessor macros from OpenSSL headers.
  # crypto.h is included because BoringSSL declares OPENSSL_IS_BORINGSSL there.
  args = ['-E', '-dM',
          '-include', 'openssl/opensslv.h',
          '-include', 'openssl/crypto.h',
          '-']
  if not options.shared_openssl:
    args = ['-I', 'deps/openssl/openssl/include'] + args
  elif options.shared_openssl_includes:
    args = ['-I', options.shared_openssl_includes] + args
  else:
    for dir in o['include_dirs']:
      args = ['-I', dir] + args

  proc = subprocess.Popen(
    shlex.split(CC) + args,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE
  )
  with proc:
    proc.stdin.write(b'\n')
    out = to_utf8(proc.communicate()[0])

  if proc.returncode != 0:
    warn('Failed to extract OpenSSL macros from headers')
    return {}

  macros = {}
  for line in out.split('\n'):
    if line.startswith('#define OPENSSL_'):
      parts = line.split()
      if len(parts) >= 2:
        macro_name = parts[1]
        macro_value = parts[2] if len(parts) >= 3 else '1'
        macros[macro_name] = macro_value

  return macros

def get_openssl_version(o):
  """Parse OpenSSL version from opensslv.h header file.

  Returns the version as a number matching OPENSSL_VERSION_NUMBER format:
  0xMNN00PPSL where M=major, NN=minor, PP=patch, S=status(0xf=release,0x0=pre),
  L denotes as a long type literal
  """

  try:
    macros = get_openssl_macros(o)

    # Extract version components
    major = int(macros.get('OPENSSL_VERSION_MAJOR', '0'))
    minor = int(macros.get('OPENSSL_VERSION_MINOR', '0'))
    patch = int(macros.get('OPENSSL_VERSION_PATCH', '0'))

    # If major, minor and patch are all 0, this is probably OpenSSL < 3.
    if (major, minor, patch) == (0, 0, 0):
      version_number = macros.get('OPENSSL_VERSION_NUMBER')
      # Prior to OpenSSL 3 the value should be in the format 0xMNN00PPSL.
      # If it is, we need to strip the `L` suffix prior to parsing.
      if version_number[:2] == "0x" and version_number[-1] == "L":
        return int(version_number[:-1], 16)

    # Check if it's a pre-release (has non-empty PRE_RELEASE string)
    pre_release = macros.get('OPENSSL_VERSION_PRE_RELEASE', '""').strip('"')
    status = 0x0 if pre_release else 0xf
    # Construct version number: 0xMNN00PPSL
    version_number = ((major << 28) |
                     (minor << 20) |
                     (patch << 4) |
                     status)

    return version_number

  except (OSError, TypeError, ValueError, subprocess.SubprocessError) as e:
    warn(f'Failed to determine OpenSSL version from header: {e}')
    return 0

def get_openssl_is_boringssl(o):
  try:
    return b('OPENSSL_IS_BORINGSSL' in get_openssl_macros(o))
  except (OSError, ValueError, subprocess.SubprocessError) as e:
    warn(f'Failed to determine whether OpenSSL headers are BoringSSL: {e}')
    return 'false'

def check_nasm_version(o):
  if not options.openssl_no_asm and options.dest_cpu in ('x86', 'x64'):
    nasm_version = get_nasm_version('nasm')
    o['variables']['nasm_version'] = nasm_version
    if nasm_version == '0.0':
      o['variables']['openssl_no_asm'] = 1

def check_assembler_version(o, is_clang):
  # Need xcode_version or gas_version when openssl asm files are compiled.
  if options.without_ssl or options.openssl_no_asm or options.shared_openssl:
    return

  if is_clang:
    if sys.platform == 'darwin':
      o['variables']['xcode_version'] = get_xcode_version(CC)
  else:
    o['variables']['gas_version'] = get_gas_version(CC)

def configure_openssl(o):
  variables = o['variables']
  variables['node_use_openssl'] = b(not options.without_ssl)
  variables['node_shared_openssl'] = b(options.shared_openssl)
  variables['node_shared_ngtcp2'] = b(options.shared_ngtcp2)
  variables['node_shared_nghttp3'] = b(options.shared_nghttp3)
  variables['openssl_is_fips'] = b(options.openssl_is_fips)

  if options.openssl_no_asm:
    variables['openssl_no_asm'] = 1

  o['defines'] += ['NODE_OPENSSL_CONF_NAME=' + options.openssl_conf_name]

  if options.without_ssl:
    def without_ssl_error(option):
      error(f'--without-ssl is incompatible with {option}')
    if options.shared_openssl:
      without_ssl_error('--shared-openssl')
    if options.openssl_no_asm:
      without_ssl_error('--openssl-no-asm')
    if options.openssl_is_fips:
      without_ssl_error('--openssl-is-fips')
    if options.openssl_default_cipher_list:
      without_ssl_error('--openssl-default-cipher-list')
    return

  if options.use_openssl_ca_store:
    o['defines'] += ['NODE_OPENSSL_CERT_STORE']
  if options.openssl_system_ca_path:
    variables['openssl_system_ca_path'] = options.openssl_system_ca_path
  variables['node_without_node_options'] = b(options.without_node_options)
  if options.without_node_options:
      o['defines'] += ['NODE_WITHOUT_NODE_OPTIONS']
  if options.openssl_default_cipher_list:
    variables['openssl_default_cipher_list'] = \
            options.openssl_default_cipher_list

  if not options.shared_openssl and not options.openssl_no_asm:
    is_x86 = 'x64' in variables['target_arch'] or 'ia32' in variables['target_arch']

    # Check for an assembler that supports the instructions used by OpenSSL.
    # See deps/openssl/openssl/INSTALL.md for its toolchain requirements.
    openssl_asm_supported = \
      ('gas_version' in variables and Version(variables['gas_version']) >= Version('2.23')) or \
      ('xcode_version' in variables and Version(variables['xcode_version']) >= Version('5.0')) or \
      ('llvm_version' in variables and Version(variables['llvm_version']) >= Version('3.3')) or \
      ('nasm_version' in variables and Version(variables['nasm_version']) >= Version('2.10'))

    if is_x86 and not openssl_asm_supported:
      error('''Did not find a new enough assembler, install one or build with
       --openssl-no-asm.
       Please refer to BUILDING.md''')

  elif options.openssl_no_asm:
    warn('''--openssl-no-asm will result in binaries that do not take advantage
         of modern CPU cryptographic instructions and will therefore be slower.
         Please refer to BUILDING.md''')

  if options.openssl_no_asm and options.shared_openssl:
    error('--openssl-no-asm is incompatible with --shared-openssl')

  if options.openssl_is_fips and not options.shared_openssl:
    error('--openssl-is-fips is only available with --shared-openssl')

  if options.openssl_is_fips:
    o['defines'] += ['OPENSSL_FIPS']

  configure_library('openssl', o, with_separate_include_var=True)

  o['variables']['openssl_version'] = get_openssl_version(o)
  o['variables']['openssl_is_boringssl'] = get_openssl_is_boringssl(o)

  # BoringSSL identifies itself as OpenSSL 1.1.1 and is exempt from this check.
  # A version of 0 means detection failed, which is already warned about in
  # get_openssl_version() and is caught at compile time by ncrypto.h.
  openssl_version = o['variables']['openssl_version']
  if o['variables']['openssl_is_boringssl'] == 'false' and \
     0 < openssl_version < 0x30000000:
    error('OpenSSL 1.x is no longer supported, v3.0.0 or later is required.')

def configure_quic(o):
  if options.experimental_quic and options.without_ssl:
    error('--experimental-quic is incompatible with --without-ssl')
  o['variables']['node_use_quic'] = b(options.experimental_quic)

  if not options.experimental_quic:
    for lib in ['nghttp3', 'ngtcp2']:
      shared_lib = 'shared_' + lib
      if getattr(options, shared_lib):
        error(f'--shared-{lib} requires --experimental-quic')
    return

  configure_library('nghttp3', o, pkgname='libnghttp3')
  configure_library('ngtcp2', o, pkgname='libngtcp2')

def configure_dtls(o):
  if options.experimental_dtls and options.without_ssl:
    error('--experimental-dtls is incompatible with --without-ssl')

  o['variables']['node_use_dtls'] = b(options.experimental_dtls)

def configure(o):
  configure_openssl(o)
  configure_quic(o)
  configure_dtls(o)
