#! /usr/bin/env python
import Options
import sys
import os
from os.path import join, dirname, abspath

import js2c

VERSION='0.0.1'
APPNAME='node'

srcdir = '.'
blddir = 'build'

def set_options(opt):
  # the gcc module provides a --debug-level option
  opt.tool_options('compiler_cxx')
  opt.tool_options('compiler_cc')
  opt.tool_options('ragel', tdir=".")
  opt.add_option( '--debug'
                , action='store_true'
                , default=False
                , help='Build debug variant [Default: False]'
                , dest='debug'
                )

def configure(conf):
  conf.check_tool('compiler_cxx')
  conf.check_tool('compiler_cc')
  conf.check_tool('ragel', tooldir=".")

  conf.sub_config('deps/libeio')
  conf.sub_config('deps/libev')

  # needs to match the symbols found in libeio and libev
  # __solaris
  # __linux
  # __freebsd
  # __hpux
  # __solaris
  platform_string = "__" + Options.platform
  if Options.platform == "linux2":
    platform_string = "__linux"
  conf.define(platform_string, 1)

  # liboi config
  print "--- liboi ---"
  if conf.check_cfg(package='gnutls', args='--cflags --libs', uselib_store="GNUTLS"):
    conf.define("HAVE_GNUTLS", 1)

  conf.define("HAVE_CONFIG_H", 1)

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
  conf.env.append_value('CCFLAGS', ['-DNDEBUG', '-O0', '-g'])
  conf.env.append_value('CXXFLAGS', ['-DNDEBUG', '-O0', '-g'])
  conf.write_config_header("config.h")

def build(bld):
  # Use debug environment when --enable-debug is given

  bld.add_subdirs('deps/libeio deps/libev')

  ### v8
  deps_src = join(bld.path.abspath(),"deps")
  deps_tgt = join(bld.srcnode.abspath(bld.env),"deps")
  v8dir_src = join(deps_src,"v8")
  v8dir_tgt = join(deps_tgt, "v8")
  #v8lib = bld.env["staticlib_PATTERN"] % "v8_g"
  v8lib = bld.env["staticlib_PATTERN"] % "v8"
  v8 = bld.new_task_gen(
    target=join("deps/v8",v8lib),
    #rule='cp -rf %s %s && cd %s && scons -Q mode=debug library=static snapshot=on' 
    rule='cp -rf %s %s && cd %s && scons -Q library=static snapshot=on' 
      % ( v8dir_src , deps_tgt , v8dir_tgt),
    before="cxx"
  )
  bld.env["CPPPATH_V8"] = "deps/v8/include"
  bld.env["STATICLIB_V8"] = "v8"
  #bld.env["STATICLIB_V8"] = "v8_g"
  bld.env["LIBPATH_V8"] = v8dir_tgt
  bld.env["LINKFLAGS_V8"] = "-pthread"

  ### oi
  oi = bld.new_task_gen("cc", "staticlib")
  oi.source = "deps/oi/oi_socket.c deps/oi/oi_buf.c"
  oi.includes = "deps/oi/"
  oi.name = "oi"
  oi.target = "oi"
  oi.uselib = "GNUTLS"

  ### ebb
  ebb = bld.new_task_gen("cc", "staticlib")
  ebb.source = "deps/ebb/ebb_request_parser.rl"
  ebb.includes = "deps/ebb/"
  ebb.name = "ebb"
  ebb.target = "ebb"

  ### src/native.cc
  def javascript_in_c(task):
    env = task.env
    source = map(lambda x: x.srcpath(env), task.inputs)
    targets = map(lambda x: x.srcpath(env), task.outputs)
    js2c.JS2C(source, targets)

  native_cc = bld.new_task_gen(
    source="src/file.js src/main.js",
    target="src/natives.h",
    rule=javascript_in_c,
    before="cxx"
  )


  ### node
  node = bld.new_task_gen("cxx", "program")
  node.target = 'node'
  node.source = """
    src/node.cc
    src/http.cc
    src/net.cc
    src/process.cc
    src/file.cc
    src/timers.cc
  """
  node.includes = """
    src/ 
    deps/v8/include
    deps/libev
    deps/libeio
    deps/oi 
    deps/ebb
  """
  node.uselib_local = "oi ev eio ebb"
  node.uselib = "V8"

  if Options.options.debug:
    print "debug build!"
    bld.env = bld.env_of_name('debug')
