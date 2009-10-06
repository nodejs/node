# /usr/bin/env python
import platform
import re
import Options
import sys, os, shutil
from os.path import join, dirname, abspath
from logging import fatal

VERSION="0.1.13"
APPNAME="node.js"

import js2c

srcdir = '.'
blddir = 'build'
cwd = os.getcwd()

def set_options(opt):
  # the gcc module provides a --debug-level option
  opt.tool_options('compiler_cxx')
  opt.tool_options('compiler_cc')
  opt.tool_options('misc')
  opt.add_option( '--debug'
                , action='store_true'
                , default=False
                , help='Build debug variant [Default: False]'
                , dest='debug'
                )
  opt.add_option( '--efence'
                , action='store_true'
                , default=False
                , help='Build with -lefence for debugging [Default: False]'
                , dest='efence'
                )

def mkdir_p(dir):
  if not os.path.exists (dir):
    os.makedirs (dir)

# Copied from Python 2.6 because 2.4.4 at least is broken by not using
# mkdirs
# http://mail.python.org/pipermail/python-bugs-list/2005-January/027118.html
def copytree(src, dst, symlinks=False, ignore=None):
    names = os.listdir(src)
    if ignore is not None:
        ignored_names = ignore(src, names)
    else:
        ignored_names = set()

    os.makedirs(dst)
    errors = []
    for name in names:
        if name in ignored_names:
            continue
        srcname = os.path.join(src, name)
        dstname = os.path.join(dst, name)
        try:
            if symlinks and os.path.islink(srcname):
                linkto = os.readlink(srcname)
                os.symlink(linkto, dstname)
            elif os.path.isdir(srcname):
                copytree(srcname, dstname, symlinks, ignore)
            else:
                shutil.copy2(srcname, dstname)
            # XXX What about devices, sockets etc.?
        except (IOError, os.error), why:
            errors.append((srcname, dstname, str(why)))
        # catch the Error from the recursive copytree so that we can
        # continue with other files
        except Error, err:
            errors.extend(err.args[0])
    try:
        shutil.copystat(src, dst)
    except OSError, why:
        if WindowsError is not None and isinstance(why, WindowsError):
            # Copying file access times may fail on Windows
            pass
        else:
            errors.extend((src, dst, str(why)))
    if errors:
        raise Error, errors

def conf_subproject (conf, subdir, command=None):
  print("---- %s ----" % subdir)
  src = join(conf.srcdir, subdir)
  if not os.path.exists (src): fatal("no such subproject " + subdir)

  default_tgt = join(conf.blddir, "default", subdir)

  if not os.path.exists(default_tgt):
    copytree(src, default_tgt, True)

  if command:
    if os.system("cd %s && %s" % (default_tgt, command)) != 0:
      fatal("Configuring %s failed." % (subdir))

  debug_tgt = join(conf.blddir, "debug", subdir)

  if not os.path.exists(debug_tgt):
    copytree(default_tgt, debug_tgt, True)

def configure(conf):
  conf.check_tool('compiler_cxx')
  conf.check_tool('compiler_cc')

  conf.env["USE_DEBUG"] = Options.options.debug

  conf.check(lib='dl', uselib_store='DL')
  conf.env.append_value("CCFLAGS", "-rdynamic")
  conf.env.append_value("LINKFLAGS_DL", "-rdynamic")

  #if Options.options.debug:
  #  conf.check(lib='profiler', uselib_store='PROFILER')

  #if Options.options.efence:
  #  conf.check(lib='efence', libpath=['/usr/lib', '/usr/local/lib'], uselib_store='EFENCE')

  if sys.platform.startswith("freebsd"):
    if not conf.check(lib="execinfo", libpath=['/usr/lib', '/usr/local/lib'], uselib_store="EXECINFO"):
      fatal("Install the libexecinfo port from /usr/ports/devel/libexecinfo.")

  conf.sub_config('deps/libeio')
  conf.sub_config('deps/libev')

  conf_subproject(conf, 'deps/udns', './configure')
  conf_subproject(conf, 'deps/v8')

  # Not using TLS yet
  # if conf.check_cfg(package='gnutls', args='--cflags --libs', uselib_store="GNUTLS"):
  #   conf.define("HAVE_GNUTLS", 1)

  conf.define("HAVE_CONFIG_H", 1)

  conf.env.append_value("CCFLAGS", "-DX_STACKSIZE=%d" % (1024*64))

  # Split off debug variant before adding variant specific defines
  debug_env = conf.env.copy()
  conf.set_env_name('debug', debug_env)

  # Configure debug variant
  conf.setenv('debug')
  debug_env.set_variant('debug')
  debug_env.append_value('CCFLAGS', ['-DDEBUG', '-g', '-O0', '-Wall', '-Wextra'])
  debug_env.append_value('CXXFLAGS', ['-DDEBUG', '-g', '-O0', '-Wall', '-Wextra'])
  conf.write_config_header("config.h")

  # Configure default variant
  conf.setenv('default')
  conf.env.append_value('CCFLAGS', ['-DNDEBUG', '-O3'])
  conf.env.append_value('CXXFLAGS', ['-DNDEBUG', '-O3'])
  conf.write_config_header("config.h")

def build_udns(bld):
  default_build_dir = bld.srcnode.abspath(bld.env_of_name("default"))

  default_dir = join(default_build_dir, "deps/udns")

  static_lib = bld.env["staticlib_PATTERN"] % "udns"

  rule = 'cd %s && make'

  default = bld.new_task_gen(
    target= join("deps/udns", static_lib),
    rule= rule % default_dir,
    before= "cxx",
    install_path= None
  )

  bld.env["CPPPATH_UDNS"] = "deps/udns"
  bld.env["STATICLIB_UDNS"] = "udns"

  bld.env_of_name('default')["STATICLIB_UDNS"] = "udns"
  bld.env_of_name('default')["LIBPATH_UDNS"] = default_dir

  if bld.env["USE_DEBUG"]:
    debug_build_dir = bld.srcnode.abspath(bld.env_of_name("debug"))
    debug_dir = join(debug_build_dir, "deps/udns")
    debug = default.clone("debug")
    debug.rule = rule % debug_dir
    #debug.target = join(debug_dir, static_lib)
    bld.env_of_name('debug')["STATICLIB_UDNS"] = "udns"
    bld.env_of_name('debug')["LIBPATH_UDNS"] = debug_dir
  bld.install_files('${PREFIX}/include/node/', 'deps/udns/udns.h')

# XXX Remove this when v8 defaults x86_64 to native builds
def GuessArchitecture():
  id = platform.machine()
  arch = platform.architecture()[0]
  if id.startswith('arm'):
    return 'arm'
  elif ('64' in id) or ('64' in arch):
    return 'x64'
  elif (not id) or (not re.match('(x|i[3-6])86', id) is None):
    return 'ia32'
  else:
    return None


def build_v8(bld):
  deps_src = join(bld.path.abspath(),"deps")
  deps_tgt = join(bld.srcnode.abspath(bld.env_of_name("default")),"deps")
  v8dir_src = join(deps_src,"v8")
  v8dir_tgt = join(deps_tgt, "v8")
  scons = os.path.join(cwd, 'tools/scons/scons.py')

  # NOTE: We want to compile V8 to export its symbols. I.E. Do not want
  # -fvisibility=hidden. When using dlopen() it seems that the loaded DSO
  # cannot see symbols in the executable which are hidden, even if the
  # executable is statically linked together...
  v8rule = 'cd %s && ' \
           'python %s -Q visibility=default mode=%s %s library=static snapshot=on'

  arch = ""
  if GuessArchitecture() == "x64":
    arch = "arch=x64"

  v8 = bld.new_task_gen(
    target = join("deps/v8", bld.env["staticlib_PATTERN"] % "v8"),
    rule=v8rule % (v8dir_tgt, scons, "release", arch),
    before="cxx",
    install_path = None
  )
  bld.env["CPPPATH_V8"] = "deps/v8/include"
  bld.env_of_name('default')["STATICLIB_V8"] = "v8"
  bld.env_of_name('default')["LIBPATH_V8"] = v8dir_tgt
  bld.env_of_name('default')["LINKFLAGS_V8"] = ["-pthread"]

  ### v8 debug
  if bld.env["USE_DEBUG"]:
    deps_tgt = join(bld.srcnode.abspath(bld.env_of_name("debug")),"deps")
    v8dir_tgt = join(deps_tgt, "v8")

    v8_debug = v8.clone("debug")
    bld.env_of_name('debug')["STATICLIB_V8"] = "v8_g"
    bld.env_of_name('debug')["LIBPATH_V8"] = v8dir_tgt
    bld.env_of_name('debug')["LINKFLAGS_V8"] = ["-pthread"]
    v8_debug.rule = v8rule % (v8dir_tgt, scons, "debug", arch)
    v8_debug.target = join("deps/v8", bld.env["staticlib_PATTERN"] % "v8_g")

  bld.install_files('${PREFIX}/include/node/', 'deps/v8/include/v8*')

def build(bld):
  bld.add_subdirs('deps/libeio deps/libev')

  build_udns(bld)
  build_v8(bld)

  ### evcom
  evcom = bld.new_task_gen("cc", "staticlib")
  evcom.source = "deps/evcom/evcom.c"
  evcom.includes = "deps/evcom/ deps/libev/"
  evcom.name = "evcom"
  evcom.target = "evcom"
  # evcom.uselib = "GNUTLS"
  evcom.install_path = None
  if bld.env["USE_DEBUG"]:
    evcom.clone("debug")
  bld.install_files('${PREFIX}/include/node/', 'deps/evcom/evcom.h')

  ### http_parser
  http_parser = bld.new_task_gen("cc", "staticlib")
  http_parser.source = "deps/http_parser/http_parser.c"
  http_parser.includes = "deps/http_parser/"
  http_parser.name = "http_parser"
  http_parser.target = "http_parser"
  http_parser.install_path = None
  if bld.env["USE_DEBUG"]:
    http_parser.clone("debug")

  ### coupling
  coupling = bld.new_task_gen("cc", "staticlib")
  coupling.source = "deps/coupling/coupling.c"
  coupling.includes = "deps/coupling/"
  coupling.name = "coupling"
  coupling.target = "coupling"
  coupling.install_path = None
  if bld.env["USE_DEBUG"]:
    coupling.clone("debug")

  ### src/native.cc
  def javascript_in_c(task):
    env = task.env
    source = map(lambda x: x.srcpath(env), task.inputs)
    targets = map(lambda x: x.srcpath(env), task.outputs)
    js2c.JS2C(source, targets)

  native_cc = bld.new_task_gen(
    source = """
      src/util.js
      src/events.js
      src/file.js
      src/node.js
    """,
    target="src/natives.h",
    rule=javascript_in_c,
    before="cxx"
  )
  native_cc.install_path = None
  if bld.env["USE_DEBUG"]:
    native_cc.clone("debug")

  ### node lib
  node = bld.new_task_gen("cxx", "program")
  node.name         = "node"
  node.target       = "node"
  node.source = """
    src/node.cc
    src/events.cc
    src/http.cc
    src/net.cc
    src/node_stdio.cc
    src/dns.cc
    src/file.cc
    src/signal_handler.cc
    src/timer.cc
    src/child_process.cc
    src/constants.cc
  """
  node.includes = """
    src/ 
    deps/v8/include
    deps/libev
    deps/udns
    deps/libeio
    deps/evcom 
    deps/http_parser
    deps/coupling
  """
  node.uselib_local = "evcom ev eio http_parser coupling"
  node.uselib = "UDNS V8 EXECINFO DL"
  node.install_path = '${PREFIX}/lib'
  node.install_path = '${PREFIX}/bin'
  node.chmod = 0755

  def subflags(program):
    x = { 'CCFLAGS'   : " ".join(program.env["CCFLAGS"])
        , 'CPPFLAGS'  : " ".join(program.env["CPPFLAGS"])
        , 'LIBFLAGS'  : " ".join(program.env["LIBFLAGS"])
        , 'VERSION'   : VERSION
        , 'PREFIX'    : program.env["PREFIX"]
        }
    return x

  # process file.pc.in -> file.pc
  node_version = bld.new_task_gen('subst', before="cxx")
  node_version.source = 'src/node_version.h.in'
  node_version.target = 'src/node_version.h'
  node_version.dict = subflags(node)
  node_version.install_path = '${PREFIX}/include/node'

  if bld.env["USE_DEBUG"]:
    node_g = node.clone("debug")
    node_g.target = "node_g"
    
    node_version_g = node_version.clone("debug")
    node_version_g.dict = subflags(node_g)
    node_version_g.install_path = None


  bld.install_files('${PREFIX}/include/node/', """
    config.h
    src/node.h
    src/object_wrap.h
    src/events.h
    src/net.h
  """)

  # Only install the man page if it exists. 
  # Do 'make doc install' to build and install it.
  if os.path.exists('doc/node.1'):
    bld.install_files('${PREFIX}/share/man/man1/', 'doc/node.1')

  bld.install_files('${PREFIX}/bin/', 'bin/*', chmod=0755)

  # Why am I using two lines? Because WAF SUCKS.
  bld.install_files('${PREFIX}/lib/node/wafadmin', 'tools/wafadmin/*.py')
  bld.install_files('${PREFIX}/lib/node/wafadmin/Tools', 'tools/wafadmin/Tools/*.py')

  bld.install_files('${PREFIX}/lib/node/libraries/', 'lib/*.js')
