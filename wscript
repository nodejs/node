# /usr/bin/env python
import re
import Options
import sys, os, shutil
from Utils import cmd_output
from os.path import join, dirname, abspath
from logging import fatal

cwd = os.getcwd()
VERSION="0.1.21"
APPNAME="node.js"

import js2c

srcdir = '.'
blddir = 'build'

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
        srcname = join(src, name)
        dstname = join(dst, name)
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
  if not os.path.exists (src): conf.fatal("no such subproject " + subdir)

  default_tgt = join(conf.blddir, "default", subdir)

  if not os.path.exists(default_tgt):
    copytree(src, default_tgt, True)

  if command:
    if os.system("cd %s && %s" % (default_tgt, command)) != 0:
      conf.fatal("Configuring %s failed." % (subdir))

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

  if not conf.check(lib="execinfo", libpath=['/usr/lib', '/usr/local/lib'], uselib_store="EXECINFO"):
    # Note on Darwin/OS X: This will fail, but will still be used as the
    # execinfo stuff are part of the standard library.
    if sys.platform.startswith("freebsd"):
      conf.fatal("Install the libexecinfo port from /usr/ports/devel/libexecinfo.")

  if conf.check_cfg(package='gnutls',
                    args='--cflags --libs',
                    atleast_version='2.5.0',
                    #libpath=['/usr/lib', '/usr/local/lib'],
                    uselib_store='GNUTLS'):
    if conf.check(lib='gpg-error',
                  #libpath=['/usr/lib', '/usr/local/lib'],
                  uselib_store='GPGERROR'):
      conf.env.append_value("CCFLAGS", "-DEVCOM_HAVE_GNUTLS=1")
      conf.env.append_value("CXXFLAGS", "-DEVCOM_HAVE_GNUTLS=1")

  conf.sub_config('deps/libeio')
  conf.sub_config('deps/libev')

  conf_subproject(conf, 'deps/udns', './configure')

  conf.define("HAVE_CONFIG_H", 1)

  conf.env.append_value("CCFLAGS", "-DX_STACKSIZE=%d" % (1024*64))

  # LFS
  conf.env.append_value('CCFLAGS',  '-D_LARGEFILE_SOURCE')
  conf.env.append_value('CXXFLAGS', '-D_LARGEFILE_SOURCE')
  conf.env.append_value('CCFLAGS',  '-D_FILE_OFFSET_BITS=64')
  conf.env.append_value('CXXFLAGS', '-D_FILE_OFFSET_BITS=64')

  # platform
  platform_def = '-DPLATFORM=' + sys.platform
  conf.env.append_value('CCFLAGS', platform_def)
  conf.env.append_value('CXXFLAGS', platform_def)

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
  t = join(bld.srcnode.abspath(bld.env_of_name("default")), default.target)
  bld.env_of_name('default')["LINKFLAGS_UDNS"] = [t]

  if bld.env["USE_DEBUG"]:
    debug_build_dir = bld.srcnode.abspath(bld.env_of_name("debug"))
    debug_dir = join(debug_build_dir, "deps/udns")
    debug = default.clone("debug")
    debug.rule = rule % debug_dir
    t = join(bld.srcnode.abspath(bld.env_of_name("debug")), debug.target)
    bld.env_of_name('debug')["LINKFLAGS_UDNS"] = [t]
  bld.install_files('${PREFIX}/include/node/', 'deps/udns/udns.h')

def v8_cmd(bld, variant):
  scons = join(cwd, 'tools/scons/scons.py')
  deps_src = join(bld.path.abspath(),"deps")
  v8dir_src = join(deps_src,"v8")

  # NOTE: We want to compile V8 to export its symbols. I.E. Do not want
  # -fvisibility=hidden. When using dlopen() it seems that the loaded DSO
  # cannot see symbols in the executable which are hidden, even if the
  # executable is statically linked together...

  # XXX Remove this when v8 defaults x86_64 to native builds
  arch = ""
  if bld.env['DEST_CPU'] == 'x86_64':
    arch = "arch=x64"

  if variant == "default":
    mode = "release"
  else:
    mode = "debug"

  cmd_R = 'python %s -C %s -Y %s visibility=default mode=%s %s library=static snapshot=on'

  cmd = cmd_R % ( scons
                , bld.srcnode.abspath(bld.env_of_name(variant))
                , v8dir_src
                , mode
                , arch
                )
  return cmd


def build_v8(bld):
  v8 = bld.new_task_gen(
    source        = 'deps/v8/SConstruct ' 
                  + bld.path.ant_glob('v8/include/*') 
                  + bld.path.ant_glob('v8/src/*'),
    target        = bld.env["staticlib_PATTERN"] % "v8",
    rule          = v8_cmd(bld, "default"),
    before        = "cxx",
    install_path  = None
  )
  v8.uselib = "EXECINFO"
  bld.env["CPPPATH_V8"] = "deps/v8/include"
  t = join(bld.srcnode.abspath(bld.env_of_name("default")), v8.target)
  bld.env_of_name('default')["LINKFLAGS_V8"] = ["-pthread", t]

  ### v8 debug
  if bld.env["USE_DEBUG"]:
    v8_debug = v8.clone("debug")
    v8_debug.rule   = v8_cmd(bld, "debug")
    v8_debug.target = bld.env["staticlib_PATTERN"] % "v8_g"
    v8_debug.uselib = "EXECINFO"
    t = join(bld.srcnode.abspath(bld.env_of_name("debug")), v8_debug.target)
    bld.env_of_name('debug')["LINKFLAGS_V8"] = ["-pthread", t]

  bld.install_files('${PREFIX}/include/node/', 'deps/v8/include/*.h')

def build(bld):
  bld.add_subdirs('deps/libeio deps/libev')

  build_udns(bld)
  build_v8(bld)

  ### evcom
  evcom = bld.new_task_gen("cc")
  evcom.source = "deps/evcom/evcom.c"
  evcom.includes = "deps/evcom/ deps/libev/"
  evcom.name = "evcom"
  evcom.target = "evcom"
  evcom.uselib = "GPGERROR GNUTLS"
  evcom.install_path = None
  if bld.env["USE_DEBUG"]:
    evcom.clone("debug")
  bld.install_files('${PREFIX}/include/node/', 'deps/evcom/evcom.h')

  ### http_parser
  http_parser = bld.new_task_gen("cc")
  http_parser.source = "deps/http_parser/http_parser.c"
  http_parser.includes = "deps/http_parser/"
  http_parser.name = "http_parser"
  http_parser.target = "http_parser"
  http_parser.install_path = None
  if bld.env["USE_DEBUG"]:
    http_parser.clone("debug")

  ### coupling
  coupling = bld.new_task_gen("cc")
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
    source='src/node.js',
    target="src/node_natives.h",
    before="cxx"
  )
  native_cc.install_path = None

  # Add the rule /after/ cloning the debug
  # This is a work around for an error had in python 2.4.3 (I'll paste the
  # error that was had into the git commit meessage. git-blame to find out
  # where.)
  if bld.env["USE_DEBUG"]:
    native_cc_debug = native_cc.clone("debug")
    native_cc_debug.rule = javascript_in_c
  native_cc.rule = javascript_in_c

  ### node lib
  node = bld.new_task_gen("cxx", "program")
  node.name         = "node"
  node.target       = "node"
  node.source = """
    src/node.cc
    src/node_child_process.cc
    src/node_constants.cc
    src/node_dns.cc
    src/node_events.cc
    src/node_file.cc
    src/node_http.cc
    src/node_net.cc
    src/node_signal_handler.cc
    src/node_stat.cc
    src/node_stdio.cc
    src/node_timer.cc
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
  node.add_objects = 'ev eio evcom http_parser coupling'
  node.uselib_local = ''
  node.uselib = 'UDNS V8 EXECINFO DL GPGERROR GNUTLS'

  node.install_path = '${PREFIX}/lib'
  node.install_path = '${PREFIX}/bin'
  node.chmod = 0755

  def subflags(program):
    if os.path.exists(join(cwd, ".git")):
      actual_version=cmd_output("git describe").strip()
    else:
      actual_version=VERSION

    x = { 'CCFLAGS'   : " ".join(program.env["CCFLAGS"])
        , 'CPPFLAGS'  : " ".join(program.env["CPPFLAGS"])
        , 'LIBFLAGS'  : " ".join(program.env["LIBFLAGS"])
        , 'VERSION'   : actual_version
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
    src/node_object_wrap.h
    src/node_events.h
    src/node_net.h
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

def shutdown():
  Options.options.debug
  # HACK to get binding.node out of build directory.
  # better way to do this?
  if not Options.commands['clean']:
    if os.path.exists('build/default/node') and not os.path.exists('node'):
      os.symlink('build/default/node', 'node')
    if os.path.exists('build/debug/node_g') and not os.path.exists('node_g'):
      os.symlink('build/debug/node_g', 'node_g')
  else:
    if os.path.exists('node'): os.unlink('node')
    if os.path.exists('node_g'): os.unlink('node_g')
