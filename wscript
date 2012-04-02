#!/usr/bin/env python

# Copyright Joyent, Inc. and other Node contributors.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to permit
# persons to whom the Software is furnished to do so, subject to the
# following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
# NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
# USE OR OTHER DEALINGS IN THE SOFTWARE.

import re
import Options
import sys, os, shutil, glob
import Utils
from Utils import cmd_output
from os.path import join, dirname, abspath, normpath
from logging import fatal

cwd = os.getcwd()
APPNAME="node.js"

# Use the directory that this file is found in to find the tools
# directory where the js2c.py file can be found.
sys.path.append(sys.argv[0] + '/tools');
import js2c

if sys.platform.startswith("cygwin"):
  print "cygwin not supported"
  sys.exit(1)

srcdir = '.'
blddir = 'out'
supported_archs = ('arm', 'ia32', 'x64') # 'mips' supported by v8, but not node

jobs=1
if os.environ.has_key('JOBS'):
  jobs = int(os.environ['JOBS'])

def safe_path(path):
  return path.replace("\\", "/")

def canonical_cpu_type(arch):
  m = {'x86': 'ia32', 'i386':'ia32', 'x86_64':'x64', 'amd64':'x64'}
  if arch in m: arch = m[arch]
  if not arch in supported_archs:
    raise Exception("supported architectures are "+', '.join(supported_archs)+\
                    " but NOT '" + arch + "'.")
  return arch

def set_options(opt):
  # the gcc module provides a --debug-level option
  opt.tool_options('compiler_cxx')
  opt.tool_options('compiler_cc')
  opt.tool_options('misc')
  opt.add_option( '--libdir'
                , action='store'
                , type='string'
                , default=False
                , help='Install into this libdir [Release: ${PREFIX}/lib]'
                )
  opt.add_option( '--debug'
                , action='store_true'
                , default=False
                , help='Build debug variant [Release: False]'
                , dest='debug'
                )
  opt.add_option( '--profile'
                , action='store_true'
                , default=False
                , help='Enable profiling [Release: False]'
                , dest='profile'
                )
  opt.add_option( '--efence'
                , action='store_true'
                , default=False
                , help='Build with -lefence for debugging [Release: False]'
                , dest='efence'
                )

  opt.add_option( '--without-npm'
                , action='store_true'
                , default=False
                , help='Don\'t install the bundled npm package manager [Release: False]'
                , dest='without_npm'
                )

  opt.add_option( '--without-snapshot'
                , action='store_true'
                , default=False
                , help='Build without snapshotting V8 libraries. You might want to set this for cross-compiling. [Release: False]'
                , dest='without_snapshot'
                )

  opt.add_option( '--without-ssl'
                , action='store_true'
                , default=False
                , help='Build without SSL'
                , dest='without_ssl'
                )


  opt.add_option('--shared-v8'
                , action='store_true'
                , default=False
                , help='Link to a shared V8 DLL instead of static linking'
                , dest='shared_v8'
                )

  opt.add_option( '--shared-v8-includes'
                , action='store'
                , default=False
                , help='Directory containing V8 header files'
                , dest='shared_v8_includes'
                )

  opt.add_option( '--shared-v8-libpath'
                , action='store'
                , default=False
                , help='A directory to search for the shared V8 DLL'
                , dest='shared_v8_libpath'
                )

  opt.add_option( '--shared-v8-libname'
                , action='store'
                , default=False
                , help="Alternative lib name to link to (default: 'v8')"
                , dest='shared_v8_libname'
                )

  opt.add_option( '--openssl-includes'
                , action='store'
                , default=False
                , help='A directory to search for the OpenSSL includes'
                , dest='openssl_includes'
                )

  opt.add_option( '--openssl-libpath'
                , action='store'
                , default=False
                , help="A directory to search for the OpenSSL libraries"
                , dest='openssl_libpath'
                )

  opt.add_option( '--no-ssl2'
                , action='store_true'
                , default=False
                , help="Disable OpenSSL v2"
                , dest='openssl_nov2'
                )

  opt.add_option( '--gdb'
                , action='store_true'
                , default=False
                , help="add gdb support"
                , dest='use_gdbjit'
                )


  opt.add_option( '--shared-zlib'
                , action='store_true'
                , default=False
                , help='Link to a shared zlib DLL instead of static linking'
                , dest='shared_zlib'
                )

  opt.add_option( '--shared-zlib-includes'
                , action='store'
                , default=False
                , help='Directory containing zlib header files'
                , dest='shared_zlib_includes'
                )

  opt.add_option( '--shared-zlib-libpath'
                , action='store'
                , default=False
                , help='A directory to search for the shared zlib DLL'
                , dest='shared_zlib_libpath'
                )


  opt.add_option( '--shared-cares'
                , action='store_true'
                , default=False
                , help='Link to a shared C-Ares DLL instead of static linking'
                , dest='shared_cares'
                )

  opt.add_option( '--shared-cares-includes'
                , action='store'
                , default=False
                , help='Directory containing C-Ares header files'
                , dest='shared_cares_includes'
                )

  opt.add_option( '--shared-cares-libpath'
                , action='store'
                , default=False
                , help='A directory to search for the shared C-Ares DLL'
                , dest='shared_cares_libpath'
                )


  opt.add_option( '--with-dtrace'
                , action='store_true'
                , default=False
                , help='Build with DTrace (experimental)'
                , dest='dtrace'
                )
 

  opt.add_option( '--product-type'
                , action='store'
                , default='program'
                , help='What kind of product to produce (program, cstaticlib '\
                       'or cshlib) [default: %default]'
                , dest='product_type'
                )

  opt.add_option( '--dest-cpu'
                , action='store'
                , default=None
                , help='CPU architecture to build for. Valid values are: '+\
                       ', '.join(supported_archs)
                , dest='dest_cpu'
                )

def get_node_version():
  def get_define_value(lines, define):
    for line in lines:
      if define in line:
        return line.split()[-1] #define <NAME> <VALUE>

  lines = open("src/node_version.h").readlines()
  node_major_version = get_define_value(lines, 'NODE_MAJOR_VERSION')
  node_minor_version = get_define_value(lines, 'NODE_MINOR_VERSION')
  node_patch_version = get_define_value(lines, 'NODE_PATCH_VERSION')
  node_is_release    = get_define_value(lines, 'NODE_VERSION_IS_RELEASE')

  return "%s.%s.%s%s" % ( node_major_version,
                           node_minor_version,
                           node_patch_version,
                           node_is_release == "0" and "-pre" or ""
                         )



def configure(conf):
  conf.check_tool('compiler_cxx')
  if not conf.env.CXX: conf.fatal('c++ compiler not found')
  conf.check_tool('compiler_cc')
  if not conf.env.CC: conf.fatal('c compiler not found')

  o = Options.options

  if o.libdir:
    conf.env['LIBDIR'] = o.libdir
  else:
    conf.env['LIBDIR'] = conf.env['PREFIX'] + '/lib'

  conf.env["USE_DEBUG"] = o.debug
  # Snapshot building does noet seem to work on mingw32
  conf.env["SNAPSHOT_V8"] = not o.without_snapshot and not sys.platform.startswith("win32")
  if sys.platform.startswith("sunos"):
    conf.env["SNAPSHOT_V8"] = False
  conf.env["USE_PROFILING"] = o.profile

  conf.env["USE_SHARED_V8"] = o.shared_v8 or o.shared_v8_includes or o.shared_v8_libpath or o.shared_v8_libname
  conf.env["USE_SHARED_CARES"] = o.shared_cares or o.shared_cares_includes or o.shared_cares_libpath
  conf.env["USE_SHARED_ZLIB"] = o.shared_zlib or o.shared_zlib_includes or o.shared_zlib_libpath

  conf.env["USE_GDBJIT"] = o.use_gdbjit
  conf.env['USE_NPM'] = not o.without_npm

  if not conf.env["USE_SHARED_ZLIB"] and not sys.platform.startswith("win32"):
    conf.env.append_value("LINKFLAGS", "-lz")

  conf.check(lib='dl', uselib_store='DL')
  if not sys.platform.startswith("sunos") and not sys.platform.startswith("win32"):
    conf.env.append_value("CCFLAGS", "-rdynamic")
    conf.env.append_value("LINKFLAGS_DL", "-rdynamic")

  if 'bsd' in sys.platform:
    conf.check(lib='kvm', uselib_store='KVM')

  #if Options.options.debug:
  #  conf.check(lib='profiler', uselib_store='PROFILER')

  if Options.options.dtrace:
    if not sys.platform.startswith("sunos"):
      conf.fatal('DTrace support only currently available on Solaris')

    conf.find_program('dtrace', var='DTRACE', mandatory=True)
    conf.env["USE_DTRACE"] = True
    conf.env.append_value("CXXFLAGS", "-DHAVE_DTRACE=1")

  if Options.options.efence:
    conf.check(lib='efence', libpath=['/usr/lib', '/usr/local/lib'], uselib_store='EFENCE')

  if 'bsd' in sys.platform:
     if not conf.check(lib="execinfo",
                       includes=['/usr/include', '/usr/local/include'],
                       libpath=['/usr/lib', '/usr/local/lib'],
                       uselib_store="EXECINFO"):
       conf.fatal("Install the libexecinfo port from /usr/ports/devel/libexecinfo.")

  if not Options.options.without_ssl:
    # Don't override explicitly supplied openssl paths with pkg-config results.
    explicit_openssl = o.openssl_includes or o.openssl_libpath

    # Disable ssl v2 methods
    if o.openssl_nov2:
      conf.env.append_value("CPPFLAGS", "-DOPENSSL_NO_SSL2=1")

    if not explicit_openssl and conf.check_cfg(package='openssl',
                                               args='--cflags --libs',
                                               uselib_store='OPENSSL'):
      Options.options.use_openssl = conf.env["USE_OPENSSL"] = True
      conf.env.append_value("CPPFLAGS", "-DHAVE_OPENSSL=1")
    else:
      if o.openssl_libpath: 
        openssl_libpath = [o.openssl_libpath]
      elif not sys.platform.startswith('win32'):
        openssl_libpath = ['/usr/lib', '/usr/local/lib', '/opt/local/lib', '/usr/sfw/lib']
      else:
        openssl_libpath = [normpath(join(cwd, '../openssl'))]

      if o.openssl_includes: 
        openssl_includes = [o.openssl_includes]
      elif not sys.platform.startswith('win32'):
        openssl_includes = [];
      else:
        openssl_includes = [normpath(join(cwd, '../openssl/include'))];

      openssl_lib_names = ['ssl', 'crypto']
      if sys.platform.startswith('win32'):
        openssl_lib_names += ['ws2_32', 'gdi32']

      libssl = conf.check_cc(lib=openssl_lib_names,
                             header_name='openssl/ssl.h',
                             function_name='SSL_library_init',
                             includes=openssl_includes,
                             libpath=openssl_libpath,
                             uselib_store='OPENSSL')

      libcrypto = conf.check_cc(lib='crypto',
                                header_name='openssl/crypto.h',
                                includes=openssl_includes,
                                libpath=openssl_libpath,
                                uselib_store='OPENSSL')

      if libcrypto and libssl:
        conf.env["USE_OPENSSL"] = Options.options.use_openssl = True
        conf.env.append_value("CPPFLAGS", "-DHAVE_OPENSSL=1")
      elif sys.platform.startswith('win32'):
        conf.fatal("Could not autodetect OpenSSL support. " +
                   "Use the --openssl-libpath and --openssl-includes options to set the search path. " +
                   "Use configure --without-ssl to disable this message.")
      else:
        conf.fatal("Could not autodetect OpenSSL support. " +
                   "Make sure OpenSSL development packages are installed. " +
                   "Use configure --without-ssl to disable this message.")
  else:
    Options.options.use_openssl = conf.env["USE_OPENSSL"] = False

  conf.check(lib='util', libpath=['/usr/lib', '/usr/local/lib'],
             uselib_store='UTIL')

  # normalize DEST_CPU from --dest-cpu, DEST_CPU or built-in value
  if Options.options.dest_cpu and Options.options.dest_cpu:
    conf.env['DEST_CPU'] = canonical_cpu_type(Options.options.dest_cpu)
  elif 'DEST_CPU' in os.environ and os.environ['DEST_CPU']:
    conf.env['DEST_CPU'] = canonical_cpu_type(os.environ['DEST_CPU'])
  elif 'DEST_CPU' in conf.env and conf.env['DEST_CPU']:
    conf.env['DEST_CPU'] = canonical_cpu_type(conf.env['DEST_CPU'])

  have_librt = conf.check(lib='rt', uselib_store='RT')

  if sys.platform.startswith("sunos"):
    code =  """
      #include <ifaddrs.h>
      int main(void) {
        struct ifaddrs hello;
        return 0;
      }
    """

    if conf.check_cc(msg="Checking for ifaddrs on solaris", fragment=code):
      conf.env.append_value('CPPFLAGS',  '-DSUNOS_HAVE_IFADDRS')

    if not conf.check(lib='socket', uselib_store="SOCKET"):
      conf.fatal("Cannot find socket library")
    if not conf.check(lib='nsl', uselib_store="NSL"):
      conf.fatal("Cannot find nsl library")
    if not conf.check(lib='kstat', uselib_store="KSTAT"):
      conf.fatal("Cannot find kstat library")

  if conf.env['USE_SHARED_V8']:
    v8_includes = [];
    if o.shared_v8_includes: v8_includes.append(o.shared_v8_includes);

    v8_libpath = [];
    if o.shared_v8_libpath: v8_libpath.append(o.shared_v8_libpath);

    if not o.shared_v8_libname: o.shared_v8_libname = 'v8'

    if not conf.check_cxx(lib=o.shared_v8_libname, header_name='v8.h',
                          uselib_store='V8',
                          includes=v8_includes,
                          libpath=v8_libpath):
      conf.fatal("Cannot find v8")

    if o.debug:
      if not conf.check_cxx(lib=o.shared_v8_libname + '_g', header_name='v8.h',
                            uselib_store='V8_G',
                            includes=v8_includes,
                            libpath=v8_libpath):
        conf.fatal("Cannot find v8_g")

  conf.define("HAVE_CONFIG_H", 1)

  if sys.platform.startswith("sunos"):
    conf.env.append_value ('CCFLAGS', '-threads')
    conf.env.append_value ('CXXFLAGS', '-threads')
    #conf.env.append_value ('LINKFLAGS', ' -threads')
  elif not sys.platform.startswith("win32"):
    threadflags='-pthread'
    conf.env.append_value ('CCFLAGS', threadflags)
    conf.env.append_value ('CXXFLAGS', threadflags)
    conf.env.append_value ('LINKFLAGS', threadflags)
  if sys.platform.startswith("darwin"):
    # used by platform_darwin_*.cc
    conf.env.append_value('LINKFLAGS', ['-framework','Carbon'])
    # cross compile for architecture specified by DEST_CPU
    if 'DEST_CPU' in conf.env:
      arch = conf.env['DEST_CPU']
      # map supported_archs to GCC names:
      arch_mappings = {'ia32': 'i386', 'x64': 'x86_64'}
      if arch in arch_mappings:
        arch = arch_mappings[arch]
      flags = ['-arch', arch]
      conf.env.append_value('CCFLAGS', flags)
      conf.env.append_value('CXXFLAGS', flags)
      conf.env.append_value('LINKFLAGS', flags)
  if 'DEST_CPU' in conf.env:
    arch = conf.env['DEST_CPU']
    # TODO: -m32 is only available on 64 bit machines, so check host type
    flags = None
    if arch == 'ia32':
      flags = '-m32'
    if flags:
      conf.env.append_value('CCFLAGS', flags)
      conf.env.append_value('CXXFLAGS', flags)
      conf.env.append_value('LINKFLAGS', flags)

  # LFS
  conf.env.append_value('CPPFLAGS',  '-D_LARGEFILE_SOURCE')
  conf.env.append_value('CPPFLAGS',  '-D_FILE_OFFSET_BITS=64')

  if sys.platform.startswith('darwin'):
    conf.env.append_value('CPPFLAGS', '-D_DARWIN_USE_64_BIT_INODE=1')

  # Makes select on windows support more than 64 FDs
  if sys.platform.startswith("win32"):
    conf.env.append_value('CPPFLAGS', '-DFD_SETSIZE=1024');

  ## needed for node_file.cc fdatasync
  ## Strangely on OSX 10.6 the g++ doesn't see fdatasync but gcc does?
  code =  """
    #include <unistd.h>
    int main(void)
    {
       int fd = 0;
       fdatasync (fd);
       return 0;
    }
  """
  if conf.check_cxx(msg="Checking for fdatasync(2) with c++", fragment=code):
    conf.env.append_value('CPPFLAGS', '-DHAVE_FDATASYNC=1')
  else:
    conf.env.append_value('CPPFLAGS', '-DHAVE_FDATASYNC=0')

  # arch
  conf.env.append_value('CPPFLAGS', '-DARCH="' + conf.env['DEST_CPU'] + '"')

  # platform
  conf.env.append_value('CPPFLAGS', '-DPLATFORM="' + conf.env['DEST_OS'] + '"')

  # posix?
  if not sys.platform.startswith('win'):
    conf.env.append_value('CPPFLAGS', '-D__POSIX__=1')

  platform_file = "src/platform_%s.cc" % conf.env['DEST_OS']
  if os.path.exists(join(cwd, platform_file)):
    Options.options.platform_file = True
    conf.env["PLATFORM_FILE"] = platform_file
  else:
    Options.options.platform_file = False
    conf.env["PLATFORM_FILE"] = "src/platform_none.cc"

  if conf.env['USE_PROFILING'] == True:
    conf.env.append_value('CPPFLAGS', '-pg')
    conf.env.append_value('LINKFLAGS', '-pg')

  if sys.platform.startswith("win32"):
    conf.env.append_value('LIB', 'psapi')
    conf.env.append_value('LIB', 'winmm')
    # This enforces ws2_32 to be linked after crypto, otherwise the linker
    # will run into undefined references from libcrypto.a
    if not Options.options.use_openssl:
      conf.env.append_value('LIB', 'ws2_32')

  conf.env.append_value('CPPFLAGS', '-Wno-unused-parameter');
  conf.env.append_value('CPPFLAGS', '-D_FORTIFY_SOURCE=2');

  # Split off debug variant before adding variant specific defines
  debug_env = conf.env.copy()
  conf.set_env_name('Debug', debug_env)

  # Configure debug variant
  conf.setenv('Debug')
  debug_env.set_variant('Debug')
  debug_env.append_value('CPPFLAGS', '-DDEBUG')
  debug_compile_flags = ['-g', '-O0', '-Wall', '-Wextra']
  debug_env.append_value('CCFLAGS', debug_compile_flags)
  debug_env.append_value('CXXFLAGS', debug_compile_flags)
  conf.write_config_header("config.h")

  # Configure default variant
  conf.setenv('Release')
  default_compile_flags = ['-g', '-O3']
  conf.env.append_value('CCFLAGS', default_compile_flags)
  conf.env.append_value('CXXFLAGS', default_compile_flags)
  conf.write_config_header("config.h")


def v8_cmd(bld, variant):
  scons = join(cwd, 'tools/scons/scons.py')
  deps_src = join(bld.path.abspath(),"deps")
  v8dir_src = join(deps_src,"v8")

  # NOTE: We want to compile V8 to export its symbols. I.E. Do not want
  # -fvisibility=hidden. When using dlopen() it seems that the loaded DSO
  # cannot see symbols in the executable which are hidden, even if the
  # executable is statically linked together...

  # XXX Change this when v8 defaults x86_64 to native builds
  # Possible values are (arm, ia32, x64, mips).
  arch = ""
  if bld.env['DEST_CPU']:
    arch = "arch="+bld.env['DEST_CPU']

  toolchain = "gcc"

  if variant == "Release":
    mode = "release"
  else:
    mode = "debug"

  if bld.env["SNAPSHOT_V8"]:
    snapshot = "snapshot=on"
  else:
    snapshot = ""

  cmd_R = sys.executable + ' "%s" -j %d -C "%s" -Y "%s" visibility=default mode=%s %s toolchain=%s library=static %s'

  cmd = cmd_R % ( scons
                , Options.options.jobs
                , safe_path(bld.srcnode.abspath(bld.env_of_name(variant)))
                , safe_path(v8dir_src)
                , mode
                , arch
                , toolchain
                , snapshot
                )

  if bld.env["USE_GDBJIT"]:
    cmd += ' gdbjit=on '

  if sys.platform.startswith("sunos"):
    cmd += ' toolchain=gcc strictaliasing=off'



  return ("echo '%s' && " % cmd) + cmd


def build_v8(bld):
  v8 = bld.new_task_gen(
    source        = 'deps/v8/SConstruct '
                    + bld.path.ant_glob('v8/include/*')
                    + bld.path.ant_glob('v8/src/*'),
    target        = bld.env["staticlib_PATTERN"] % "v8",
    rule          = v8_cmd(bld, "Release"),
    before        = "cxx",
    install_path  = None)

  v8.env.env = dict(os.environ)
  v8.env.env['CC'] = sh_escape(bld.env['CC'][0])
  v8.env.env['CXX'] = sh_escape(bld.env['CXX'][0])

  v8.uselib = "EXECINFO"
  bld.env["CPPPATH_V8"] = "deps/v8/include"
  t = join(bld.srcnode.abspath(bld.env_of_name("Release")), v8.target)
  bld.env_of_name('Release').append_value("LINKFLAGS_V8", t)

  ### v8 debug
  if bld.env["USE_DEBUG"]:
    v8_debug = v8.clone("Debug")
    v8_debug.rule   = v8_cmd(bld, "Debug")
    v8_debug.target = bld.env["staticlib_PATTERN"] % "v8_g"
    v8_debug.uselib = "EXECINFO"
    bld.env["CPPPATH_V8_G"] = "deps/v8/include"
    t = join(bld.srcnode.abspath(bld.env_of_name("Debug")), v8_debug.target)
    bld.env_of_name('Debug').append_value("LINKFLAGS_V8_G", t)

  bld.install_files('${PREFIX}/include/node/', 'deps/v8/include/*.h')

def sh_escape(s):
  if sys.platform.startswith('win32'):
    return '"' + s + '"'
  else:
    return s.replace("\\", "\\\\").replace("(","\\(").replace(")","\\)").replace(" ","\\ ")

def uv_cmd(bld, variant):
  srcdeps = join(bld.path.abspath(), "deps")
  srcdir = join(srcdeps, "uv")
  blddir = bld.srcnode.abspath(bld.env_of_name(variant)) + '/deps/uv'
  #
  # FIXME This is awful! We're copying the entire source directory into the
  # build directory before each compile. This could be much improved by
  # modifying libuv's build to send object files to a separate directory.
  #
  cmd = 'cp -r ' + sh_escape(srcdir)  + '/* ' + sh_escape(blddir)
  if not sys.platform.startswith('win32'):
    make = ('if [ -z "$NODE_MAKE" ]; then NODE_MAKE=make; fi; '
            '$NODE_MAKE -C ' + sh_escape(blddir))
  else:
    make = 'make -C ' + sh_escape(blddir)
  return '%s && (%s clean) && (%s all)' % (cmd, make, make)


def build_uv(bld):
  uv = bld.new_task_gen(
    name = 'uv',
    source = 'deps/uv/include/uv.h',
    target = 'deps/uv/uv.a',
    before = "cxx",
    rule = uv_cmd(bld, 'Release')
  )

  uv.env.env = dict(os.environ)
  uv.env.env['CC'] = sh_escape(bld.env['CC'][0])
  uv.env.env['CXX'] = sh_escape(bld.env['CXX'][0])

  t = join(bld.srcnode.abspath(bld.env_of_name("Release")), uv.target)
  bld.env_of_name('Release').append_value("LINKFLAGS_UV", t)

  if bld.env["USE_DEBUG"]:
    uv_debug = uv.clone("Debug")
    uv_debug.rule = uv_cmd(bld, 'Debug')
    uv_debug.env.env = dict(os.environ)

    t = join(bld.srcnode.abspath(bld.env_of_name("Debug")), uv_debug.target)
    bld.env_of_name('Debug').append_value("LINKFLAGS_UV", t)

  bld.install_files('${PREFIX}/include/node/', 'deps/uv/include/*.h')
  bld.install_files('${PREFIX}/include/node/uv-private', 'deps/uv/include/uv-private/*.h')
  bld.install_files('${PREFIX}/include/node/ev', 'deps/uv/src/ev/*.h')
  bld.install_files('${PREFIX}/include/node/c-ares', """
    deps/uv/include/ares.h
    deps/uv/include/ares_version.h
  """)


def build(bld):
  ## This snippet is to show full commands as WAF executes
  import Build
  old = Build.BuildContext.exec_command
  def exec_command(self, cmd, **kw):
    if isinstance(cmd, list): print(" ".join(cmd))
    return old(self, cmd, **kw)
  Build.BuildContext.exec_command = exec_command

  Options.options.jobs=jobs
  product_type = Options.options.product_type
  product_type_is_lib = product_type != 'program'

  print "DEST_OS: " + bld.env['DEST_OS']
  print "DEST_CPU: " + bld.env['DEST_CPU']
  print "Parallel Jobs: " + str(Options.options.jobs)
  print "Product type: " + product_type

  build_uv(bld)

  if not bld.env['USE_SHARED_V8']: build_v8(bld)


  ### http_parser
  http_parser = bld.new_task_gen("cc")
  http_parser.source = "deps/http_parser/http_parser.c"
  http_parser.includes = "deps/http_parser/"
  http_parser.name = "http_parser"
  http_parser.target = "http_parser"
  http_parser.install_path = None
  if bld.env["USE_DEBUG"]:
    http_parser.clone("Debug")
  if product_type_is_lib:
    http_parser.ccflags = '-fPIC'

  ### src/native.cc
  def make_macros(loc, content):
    f = open(loc, 'a')
    f.write(content)
    f.close

  macros_loc_debug   = join(
     bld.srcnode.abspath(bld.env_of_name("Debug")),
     "macros.py"
  )

  macros_loc_default = join(
    bld.srcnode.abspath(bld.env_of_name("Release")),
    "macros.py"
  )

  ### We need to truncate the macros.py file
  f = open(macros_loc_debug, 'w')
  f.close
  f = open(macros_loc_default, 'w')
  f.close

  make_macros(macros_loc_debug, "")  # leave debug(x) as is in debug build
  # replace debug(x) with nothing in release build
  make_macros(macros_loc_default, "macro debug(x) = ;\n")
  make_macros(macros_loc_default, "macro assert(x) = ;\n")

  if not bld.env["USE_DTRACE"]:
    probes = [
      'DTRACE_HTTP_CLIENT_REQUEST',
      'DTRACE_HTTP_CLIENT_RESPONSE',
      'DTRACE_HTTP_SERVER_REQUEST',
      'DTRACE_HTTP_SERVER_RESPONSE',
      'DTRACE_NET_SERVER_CONNECTION',
      'DTRACE_NET_STREAM_END',
      'DTRACE_NET_SOCKET_READ',
      'DTRACE_NET_SOCKET_WRITE'
    ]

    for probe in probes:
      make_macros(macros_loc_default, "macro %s(x) = ;\n" % probe)
      make_macros(macros_loc_debug, "macro %s(x) = ;\n" % probe)

  def javascript_in_c(task):
    env = task.env
    source = map(lambda x: x.srcpath(env), task.inputs)
    targets = map(lambda x: x.srcpath(env), task.outputs)
    source.append(macros_loc_default)
    js2c.JS2C(source, targets)

  def javascript_in_c_debug(task):
    env = task.env
    source = map(lambda x: x.srcpath(env), task.inputs)
    targets = map(lambda x: x.srcpath(env), task.outputs)
    source.append(macros_loc_debug)
    js2c.JS2C(source, targets)

  native_cc = bld.new_task_gen(
    source='src/node.js ' + bld.path.ant_glob('lib/*.js'),
    target="src/node_natives.h",
    before="cxx",
    install_path=None
  )

  # Add the rule /after/ cloning the debug
  # This is a work around for an error had in python 2.4.3 (I'll paste the
  # error that was had into the git commit meessage. git-blame to find out
  # where.)
  if bld.env["USE_DEBUG"]:
    native_cc_debug = native_cc.clone("Debug")
    native_cc_debug.rule = javascript_in_c_debug

  native_cc.rule = javascript_in_c_debug

  if bld.env["USE_DTRACE"]:
    dtrace_usdt = bld.new_task_gen(
      name   = "dtrace_usdt",
      source = "src/node_provider.d",
      target = "src/node_provider.h",
      rule   = "%s -x nolibs -h -o ${TGT} -s ${SRC}" % (bld.env.DTRACE),
      before = "cxx",
    )

    if bld.env["USE_DEBUG"]:
      dtrace_usdt_g = dtrace_usdt.clone("Debug")

    bld.install_files('${LIBDIR}/dtrace', 'src/node.d')

    if sys.platform.startswith("sunos"):
      #
      # The USDT DTrace provider works slightly differently on Solaris than on
      # the Mac; on Solaris, any objects that have USDT DTrace probes must be
      # post-processed with the DTrace command.  (This is not true on the
      # Mac, which has first-class linker support for USDT probes.)  On
      # Solaris, we must therefore post-process our object files.  Waf doesn't
      # seem to really have a notion for this, so we inject a task after
      # compiling and before linking, and then find all of the node object
      # files and shuck them off to dtrace (which will modify them in place
      # as appropriate).
      #
      def dtrace_postprocess(task):
        abspath = bld.srcnode.abspath(bld.env_of_name(task.env.variant()))
        objs = glob.glob(abspath + 'src/*.o')
        source = task.inputs[0].srcpath(task.env)
        target = task.outputs[0].srcpath(task.env)
        cmd = '%s -G -x nolibs -s %s -o %s %s' % (task.env.DTRACE,
                                                  source,
                                                  target,
                                                  ' '.join(objs))
        Utils.exec_command(cmd)

      #
      # ustack helpers do not currently work on MacOS.  We currently only
      # support 32-bit x86.
      #
      def dtrace_do_ustack(task):
        abspath = bld.srcnode.abspath(bld.env_of_name(task.env.variant()))
        source = task.inputs[0].srcpath(task.env)
        target = task.outputs[0].srcpath(task.env)
        cmd = '%s -32 -I../src -C -G -s %s -o %s' % (task.env.DTRACE, source, target)
        Utils.exec_command(cmd)

      dtrace_ustack = bld.new_task_gen(
	name = "dtrace_ustack-postprocess",
	source = "src/v8ustack.d",
	target = "v8ustack.o",
	always = True,
	before = "cxx_link",
	after = "cxx",
	rule = dtrace_do_ustack
      )

      dtracepost = bld.new_task_gen(
        name   = "dtrace_usdt-postprocess",
        source = "src/node_provider.d",
        target = "node_provider.o",
        always = True,
        before = "cxx_link",
        after  = "cxx",
        rule = dtrace_postprocess
      )

      t = join(bld.srcnode.abspath(bld.env_of_name("Release")), dtracepost.target)
      bld.env_of_name('Release').append_value('LINKFLAGS', t)

      t = join(bld.srcnode.abspath(bld.env_of_name("Release")), dtrace_ustack.target)
      bld.env_of_name('Release').append_value('LINKFLAGS', t)

      #
      # Note that for the same (mysterious) issue outlined above with respect
      # to assigning the rule to native_cc/native_cc_debug, we must apply the
      # rule to dtracepost/dtracepost_g only after they have been cloned.  We
      # also must put node_provider.o on the link line, but because we
      # (apparently?) lack LINKFLAGS in debug, we (shamelessly) stowaway on
      # LINKFLAGS_V8_G.
      #
      if bld.env["USE_DEBUG"]:
        dtracepost_g = dtracepost.clone("Debug")
        dtracepost_g.rule = dtrace_postprocess
        t = join(bld.srcnode.abspath(bld.env_of_name("Debug")), dtracepost.target)
        bld.env_of_name("Debug").append_value('LINKFLAGS_V8_G', t)


  ### node lib
  node = bld.new_task_gen("cxx", product_type)
  node.name         = "node"
  node.target       = "node"
  node.uselib = 'RT OPENSSL ZLIB CARES EXECINFO DL KVM SOCKET NSL KSTAT UTIL OPROFILE'
  node.add_objects = 'http_parser'
  if product_type_is_lib:
    node.install_path = '${LIBDIR}'
  else:
    node.install_path = '${PREFIX}/bin'
  node.chmod = 0755
  node.source = """
    src/node.cc
    src/node_buffer.cc
    src/node_javascript.cc
    src/node_extensions.cc
    src/node_http_parser.cc
    src/node_constants.cc
    src/node_file.cc
    src/node_script.cc
    src/node_os.cc
    src/node_dtrace.cc
    src/node_string.cc
    src/node_zlib.cc
    src/timer_wrap.cc
    src/handle_wrap.cc
    src/stream_wrap.cc
    src/tcp_wrap.cc
    src/udp_wrap.cc
    src/pipe_wrap.cc
    src/cares_wrap.cc
    src/tty_wrap.cc
    src/fs_event_wrap.cc
    src/process_wrap.cc
    src/v8_typed_array.cc
  """

  if bld.env["USE_DTRACE"]:
    node.source += " src/node_dtrace.cc "

  if not sys.platform.startswith("win32"):
    node.source += " src/node_signal_watcher.cc "
    node.source += " src/node_stat_watcher.cc "
    node.source += " src/node_io_watcher.cc "

  node.source += bld.env["PLATFORM_FILE"]
  if not product_type_is_lib:
    node.source = 'src/node_main.cc '+node.source

  if bld.env["USE_OPENSSL"]: node.source += " src/node_crypto.cc "

  node.includes = """
    src/
    deps/http_parser
    deps/uv/include
    deps/uv/src/ev
    deps/uv/src/ares
  """

  if not bld.env["USE_SHARED_V8"]: node.includes += ' deps/v8/include '

  if os.environ.has_key('RPATH'):
    node.rpath = os.environ['RPATH']

  if (sys.platform.startswith("win32")):
    # Static libgcc
    bld.env.append_value('LINKFLAGS', '-static-libgcc')
    bld.env.append_value('LINKFLAGS', '-static-libstdc++')

  def subflags(program):
    x = { 'CCFLAGS'   : " ".join(program.env["CCFLAGS"]).replace('"', '\\"')
        , 'CPPFLAGS'  : " ".join(program.env["CPPFLAGS"]).replace('"', '\\"')
        , 'LIBFLAGS'  : " ".join(program.env["LIBFLAGS"]).replace('"', '\\"')
        , 'PREFIX'    : safe_path(program.env["PREFIX"])
        , 'VERSION'   : get_node_version()
        }
    return x

  # process file.pc.in -> file.pc

  node_conf = bld.new_task_gen('subst', before="cxx")
  node_conf.source = 'src/node_config.h.in'
  node_conf.target = 'src/node_config.h'
  node_conf.dict = subflags(node)
  node_conf.install_path = '${PREFIX}/include/node'

  if bld.env["USE_DEBUG"]:
    node_g = node.clone("Debug")
    node_g.target = "node"
    node_g.uselib += ' V8_G UV '
    node_g.install_path = None

    node_conf_g = node_conf.clone("Debug")
    node_conf_g.dict = subflags(node_g)
    node_conf_g.install_path = None

  # After creating the debug clone, append the V8 dep
  node.uselib += ' V8 UV '

  bld.install_files('${PREFIX}/include/node/', """
    config.h
    src/node.h
    src/node_object_wrap.h
    src/node_buffer.h
    src/node_version.h
  """)

  # Only install the man page if it exists.
  # Do 'make doc install' to build and install it.
  if os.path.exists('doc/node.1'):
    prefix = 'bsd' in sys.platform and '${PREFIX}' or '${PREFIX}/share'
    bld.install_files(prefix + '/man/man1/', 'doc/node.1')

  bld.install_files('${PREFIX}/bin/', 'tools/node-waf', chmod=0755)
  bld.install_files('${LIBDIR}/node/wafadmin', 'tools/wafadmin/*.py')
  bld.install_files('${LIBDIR}/node/wafadmin/Tools', 'tools/wafadmin/Tools/*.py')

  if bld.env['USE_NPM']:
    install_npm(bld)

def install_npm(bld):
  start_dir = bld.path.find_dir('deps/npm')
  # The chmod=-1 is a Node hack. We changed WAF so that when chmod was set to
  # -1 that the same permission in this tree are used. Necessary to get
  # npm-cli.js to be executable without having to list every file in npm.
  bld.install_files('${LIBDIR}/node_modules/npm',
                    start_dir.ant_glob('**/*'),
                    cwd=start_dir,
                    relative_trick=True,
                    chmod=-1)
  bld.symlink_as('${PREFIX}/bin/npm',
                 '../lib/node_modules/npm/bin/npm-cli.js')

def shutdown():
  Options.options.debug
  # HACK to get binding.node out of build directory.
  # better way to do this?
  if Options.commands['configure']:
    if not Options.options.use_openssl:
      print "WARNING WARNING WARNING"
      print "OpenSSL not found. Will compile Node without crypto support!"

    if not Options.options.platform_file:
      print "WARNING: Platform not fully supported. Using src/platform_none.cc"

  elif not Options.commands['clean']:
    if sys.platform.startswith("win32"):
      if os.path.exists('out/Release/node.exe'):
        os.system('cp out/Release/node.exe .')
      if os.path.exists('out/Debug/node.exe'):
        os.system('cp out/Debug/node.exe node_g.exe')
    else:
      if os.path.exists('out/Release/node') and not os.path.islink('node'):
        os.symlink('out/Release/node', 'node')
      if os.path.exists('out/Debug/node') and not os.path.islink('node_g'):
        os.symlink('out/Debug/node', 'node_g')
  else:
    if sys.platform.startswith("win32"):
      if os.path.exists('node.exe'): os.unlink('node.exe')
      if os.path.exists('node_g.exe'): os.unlink('node_g.exe')
    else:
      if os.path.exists('node'): os.unlink('node')
      if os.path.exists('node_g'): os.unlink('node_g')
