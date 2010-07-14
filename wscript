#!/usr/bin/env python
import re
import Options
import sys, os, shutil
from Utils import cmd_output
from os.path import join, dirname, abspath
from logging import fatal

cwd = os.getcwd()
APPNAME="node.js"

import js2c

srcdir = '.'
blddir = 'build'


jobs=1
if os.environ.has_key('JOBS'):
  jobs = int(os.environ['JOBS'])
else:
  try:
    import multiprocessing
    jobs = multiprocessing.cpu_count()
  except:
    pass

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


  opt.add_option('--shared-cares'
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


  opt.add_option('--shared-libev'
                , action='store_true'
                , default=False
                , help='Link to a shared libev DLL instead of static linking'
                , dest='shared_libev'
                )

  opt.add_option( '--shared-libev-includes'
                , action='store'
                , default=False
                , help='Directory containing libev header files'
                , dest='shared_libev_includes'
                )

  opt.add_option( '--shared-libev-libpath'
                , action='store'
                , default=False
                , help='A directory to search for the shared libev DLL'
                , dest='shared_libev_libpath'
                )




def configure(conf):
  conf.check_tool('compiler_cxx')
  if not conf.env.CXX: conf.fatal('c++ compiler not found')
  conf.check_tool('compiler_cc')
  if not conf.env.CC: conf.fatal('c compiler not found')

  o = Options.options

  conf.env["USE_DEBUG"] = o.debug

  conf.env["USE_SHARED_V8"] = o.shared_v8 or o.shared_v8_includes or o.shared_v8_libpath or o.shared_v8_libname
  conf.env["USE_SHARED_CARES"] = o.shared_cares or o.shared_cares_includes or o.shared_cares_libpath
  conf.env["USE_SHARED_LIBEV"] = o.shared_libev or o.shared_libev_includes or o.shared_libev_libpath

  conf.check(lib='dl', uselib_store='DL')
  if not sys.platform.startswith("sunos") and not sys.platform.startswith("cygwin"):
    conf.env.append_value("CCFLAGS", "-rdynamic")
    conf.env.append_value("LINKFLAGS_DL", "-rdynamic")

  if sys.platform.startswith("freebsd"):
    conf.check(lib='kvm', uselib_store='KVM')

  #if Options.options.debug:
  #  conf.check(lib='profiler', uselib_store='PROFILER')

  if Options.options.efence:
    conf.check(lib='efence', libpath=['/usr/lib', '/usr/local/lib'], uselib_store='EFENCE')

  if not conf.check(lib="execinfo", includes=['/usr/include', '/usr/local/include'], libpath=['/usr/lib', '/usr/local/lib'], uselib_store="EXECINFO"):
    # Note on Darwin/OS X: This will fail, but will still be used as the
    # execinfo stuff are part of the standard library.
    if sys.platform.startswith("freebsd"):
      conf.fatal("Install the libexecinfo port from /usr/ports/devel/libexecinfo.")

  if not Options.options.without_ssl:
    if conf.check_cfg(package='openssl',
                      args='--cflags --libs',
                      uselib_store='OPENSSL'):
      conf.env["USE_OPENSSL"] = True
      conf.env.append_value("CXXFLAGS", "-DHAVE_OPENSSL=1")
    else:
      libssl = conf.check_cc(lib='ssl',
                             header_name='openssl/ssl.h',
                             function_name='SSL_library_init',
                             libpath=['/usr/lib', '/usr/local/lib', '/opt/local/lib', '/usr/sfw/lib'],
                             uselib_store='OPENSSL')
      libcrypto = conf.check_cc(lib='crypto',
                                header_name='openssl/crypto.h',
                                uselib_store='OPENSSL')
      if libcrypto and libssl:
        conf.env["USE_OPENSSL"] = True
        conf.env.append_value("CXXFLAGS", "-DHAVE_OPENSSL=1")

  conf.check(lib='rt', uselib_store='RT')

  if sys.platform.startswith("sunos"):
    if not conf.check(lib='socket', uselib_store="SOCKET"):
      conf.fatal("Cannot find socket library")
    if not conf.check(lib='nsl', uselib_store="NSL"):
      conf.fatal("Cannot find nsl library")



  conf.sub_config('deps/libeio')



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

  if conf.env['USE_SHARED_CARES']:
    cares_includes = [];
    if o.shared_cares_includes: cares_includes.append(o.shared_cares_includes);
    cares_libpath = [];
    if o.shared_cares_libpath: cares_libpath.append(o.shared_cares_libpath);
    if not conf.check_cxx(lib='cares',
                          header_name='ares.h',
                          uselib_store='CARES',
                          includes=cares_includes,
                          libpath=cares_libpath):
      conf.fatal("Cannot find c-ares")
  else:
    conf.sub_config('deps/c-ares')


  if conf.env['USE_SHARED_LIBEV']:
    libev_includes = [];
    if o.shared_libev_includes: libev_includes.append(o.shared_libev_includes);
    libev_libpath = [];
    if o.shared_libev_libpath: libev_libpath.append(o.shared_libev_libpath);
    if not conf.check_cxx(lib='ev', header_name='ev.h',
                          uselib_store='EV',
                          includes=libev_includes,
                          libpath=libev_libpath):
      conf.fatal("Cannot find libev")
  else:
    conf.sub_config('deps/libev')



  conf.define("HAVE_CONFIG_H", 1)

  if sys.platform.startswith("sunos"):
    conf.env.append_value ('CCFLAGS', '-threads')
    conf.env.append_value ('CXXFLAGS', '-threads')
    #conf.env.append_value ('LINKFLAGS', ' -threads')
  elif not sys.platform.startswith("cygwin"):
    threadflags='-pthread'
    conf.env.append_value ('CCFLAGS', threadflags)
    conf.env.append_value ('CXXFLAGS', threadflags)
    conf.env.append_value ('LINKFLAGS', threadflags)

  conf.env.append_value("CCFLAGS", "-DX_STACKSIZE=%d" % (1024*64))

  # LFS
  conf.env.append_value('CCFLAGS',  '-D_LARGEFILE_SOURCE')
  conf.env.append_value('CXXFLAGS', '-D_LARGEFILE_SOURCE')
  conf.env.append_value('CCFLAGS',  '-D_FILE_OFFSET_BITS=64')
  conf.env.append_value('CXXFLAGS', '-D_FILE_OFFSET_BITS=64')

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
    conf.env.append_value('CXXFLAGS', '-DHAVE_FDATASYNC=1')
  else:
    conf.env.append_value('CXXFLAGS', '-DHAVE_FDATASYNC=0')

  # platform
  platform_def = '-DPLATFORM=' + conf.env['DEST_OS']
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
  conf.env.append_value('CCFLAGS', ['-DNDEBUG', '-g', '-O3'])
  conf.env.append_value('CXXFLAGS', ['-DNDEBUG', '-g', '-O3'])
  conf.write_config_header("config.h")


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

  cmd_R = 'python "%s" -j %d -C "%s" -Y "%s" visibility=default mode=%s %s library=static snapshot=on'

  cmd = cmd_R % ( scons
                , Options.options.jobs
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
    install_path  = None)
  v8.uselib = "EXECINFO"
  bld.env["CPPPATH_V8"] = "deps/v8/include"
  t = join(bld.srcnode.abspath(bld.env_of_name("default")), v8.target)
  bld.env_of_name('default').append_value("LINKFLAGS_V8", t)


  ### v8 debug
  if bld.env["USE_DEBUG"]:
    v8_debug = v8.clone("debug")
    v8_debug.rule   = v8_cmd(bld, "debug")
    v8_debug.target = bld.env["staticlib_PATTERN"] % "v8_g"
    v8_debug.uselib = "EXECINFO"
    bld.env["CPPPATH_V8_G"] = "deps/v8/include"
    t = join(bld.srcnode.abspath(bld.env_of_name("debug")), v8_debug.target)
    bld.env_of_name('debug').append_value("LINKFLAGS_V8_G", t)

  bld.install_files('${PREFIX}/include/node/', 'deps/v8/include/*.h')


def build(bld):
  ## This snippet is to show full commands as WAF executes
  import Build
  old = Build.BuildContext.exec_command
  def exec_command(self, cmd, **kw):
    if isinstance(cmd, list): print(" ".join(cmd))
    return old(self, cmd, **kw)
  Build.BuildContext.exec_command = exec_command

  Options.options.jobs=jobs

  print "DEST_OS: " + bld.env['DEST_OS']
  print "DEST_CPU: " + bld.env['DEST_CPU']
  print "Parallel Jobs: " + str(Options.options.jobs)

  bld.add_subdirs('deps/libeio')

  if not bld.env['USE_SHARED_V8']: build_v8(bld)
  if not bld.env['USE_SHARED_LIBEV']: bld.add_subdirs('deps/libev')
  if not bld.env['USE_SHARED_CARES']: bld.add_subdirs('deps/c-ares')


  ### http_parser
  http_parser = bld.new_task_gen("cc")
  http_parser.source = "deps/http_parser/http_parser.c"
  http_parser.includes = "deps/http_parser/"
  http_parser.name = "http_parser"
  http_parser.target = "http_parser"
  http_parser.install_path = None
  if bld.env["USE_DEBUG"]:
    http_parser.clone("debug")

  ### src/native.cc
  def make_macros(loc, content):
    f = open(loc, 'w')
    f.write(content)
    f.close

  macros_loc_debug   = join(
     bld.srcnode.abspath(bld.env_of_name("debug")),
     "macros.py"
  )

  macros_loc_default = join(
    bld.srcnode.abspath(bld.env_of_name("default")),
    "macros.py"
  )

  make_macros(macros_loc_debug, "")  # leave debug(x) as is in debug build
  # replace debug(x) with nothing in release build
  make_macros(macros_loc_default, "macro debug(x) = ;\n")

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
    native_cc_debug = native_cc.clone("debug")
    native_cc_debug.rule = javascript_in_c_debug

  native_cc.rule = javascript_in_c

  ### node lib
  node = bld.new_task_gen("cxx", "program")
  node.name         = "node"
  node.target       = "node"
  node.uselib = 'RT EV OPENSSL CARES EXECINFO DL KVM SOCKET NSL'
  node.add_objects = 'eio http_parser'
  node.install_path = '${PREFIX}/lib'
  node.install_path = '${PREFIX}/bin'
  node.chmod = 0755
  node.source = """
    src/node.cc
    src/node_buffer.cc
    src/node_extensions.cc
    src/node_http_parser.cc
    src/node_net.cc
    src/node_io_watcher.cc
    src/node_child_process.cc
    src/node_constants.cc
    src/node_cares.cc
    src/node_events.cc
    src/node_file.cc
    src/node_signal_watcher.cc
    src/node_stat_watcher.cc
    src/node_stdio.cc
    src/node_timer.cc
    src/node_script.cc
  """

  platform_file = "src/platform_%s.cc" % bld.env['DEST_OS']
  if os.path.exists(join(cwd, platform_file)):
    node.source += platform_file
  else:
    node.source += "src/platform_none.cc "


  if bld.env["USE_OPENSSL"]: node.source += " src/node_crypto.cc "

  node.includes = """
    src/
    deps/libeio
    deps/http_parser
  """

  if not bld.env["USE_SHARED_V8"]: node.includes += ' deps/v8/include '

  if not bld.env["USE_SHARED_LIBEV"]:
    node.add_objects += ' ev '
    node.includes += ' deps/libev '

  if not bld.env["USE_SHARED_CARES"]:
    node.add_objects += ' cares '
    node.includes += '  deps/c-ares deps/c-ares/' + bld.env['DEST_OS'] + '-' + bld.env['DEST_CPU']

  if sys.platform.startswith('cygwin'):
    bld.env.append_value('LINKFLAGS', '-Wl,--export-all-symbols')
    bld.env.append_value('LINKFLAGS', '-Wl,--out-implib,default/libnode.dll.a')
    bld.env.append_value('LINKFLAGS', '-Wl,--output-def,default/libnode.def')
    bld.install_files('${PREFIX}/lib', "build/default/libnode.*")

  def subflags(program):
    x = { 'CCFLAGS'   : " ".join(program.env["CCFLAGS"])
        , 'CPPFLAGS'  : " ".join(program.env["CPPFLAGS"])
        , 'LIBFLAGS'  : " ".join(program.env["LIBFLAGS"])
        , 'PREFIX'    : program.env["PREFIX"]
        }
    return x

  # process file.pc.in -> file.pc

  node_conf = bld.new_task_gen('subst', before="cxx")
  node_conf.source = 'src/node_config.h.in'
  node_conf.target = 'src/node_config.h'
  node_conf.dict = subflags(node)
  node_conf.install_path = '${PREFIX}/include/node'

  if bld.env["USE_DEBUG"]:
    node_g = node.clone("debug")
    node_g.target = "node_g"
    node_g.uselib += ' V8_G'

    node_conf_g = node_conf.clone("debug")
    node_conf_g.dict = subflags(node_g)
    node_conf_g.install_path = None

  # After creating the debug clone, append the V8 dep
  node.uselib += ' V8'

  bld.install_files('${PREFIX}/include/node/', """
    config.h
    src/node.h
    src/node_object_wrap.h
    src/node_buffer.h
    src/node_events.h
  """)

  # Only install the man page if it exists.
  # Do 'make doc install' to build and install it.
  if os.path.exists('doc/node.1'):
    bld.install_files('${PREFIX}/share/man/man1/', 'doc/node.1')

  bld.install_files('${PREFIX}/bin/', 'bin/*', chmod=0755)
  bld.install_files('${PREFIX}/lib/node/wafadmin', 'tools/wafadmin/*.py')
  bld.install_files('${PREFIX}/lib/node/wafadmin/Tools', 'tools/wafadmin/Tools/*.py')

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
