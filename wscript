#! /usr/bin/env python
import Options
import sys
import os
from os.path import join, dirname, abspath
from logging import fatal


import js2c

VERSION='0.0.1'
APPNAME='node'

srcdir = '.'
blddir = 'build'

def set_options(opt):
  # the gcc module provides a --debug-level option
  opt.tool_options('compiler_cxx')
  opt.tool_options('compiler_cc')
  opt.add_option( '--debug'
                , action='store_true'
                , default=False
                , help='Build debug variant [Default: False]'
                , dest='debug'
                )

def configure(conf):
  conf.check_tool('compiler_cxx')
  conf.check_tool('compiler_cc')

  conf.env["USE_DEBUG"] = Options.options.debug

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

  conf.env.append_value("CCFLAGS", "-DEIO_STACKSIZE=%d" % (4096*8))
  conf.check(lib='rt', uselib_store='RT')

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
  conf.env.append_value('CCFLAGS', ['-DNDEBUG', '-O2'])
  conf.env.append_value('CXXFLAGS', ['-DNDEBUG', '-O2'])
  conf.write_config_header("config.h")

def build(bld):
  bld.add_subdirs('deps/libeio deps/libev')

  ### v8
  deps_src = join(bld.path.abspath(),"deps")
  deps_tgt = join(bld.srcnode.abspath(bld.env_of_name("default")),"deps")
  v8dir_src = join(deps_src,"v8")
  v8dir_tgt = join(deps_tgt, "v8")

  v8rule = 'cp -rf %s %s && ' \
           'cd %s && ' \
           'python scons.py -Q mode=%s library=static snapshot=on'

  v8 = bld.new_task_gen(
    target = join("deps/v8", bld.env["staticlib_PATTERN"] % "v8"),
    rule=v8rule % ( v8dir_src , deps_tgt , v8dir_tgt, "release"),
    before="cxx",
    install_path = None
  )
  bld.env["CPPPATH_V8"] = "deps/v8/include"
  bld.env["LINKFLAGS_V8"] = "-pthread"
  bld.env_of_name('default')["STATICLIB_V8"] = "v8"
  bld.env_of_name('default')["LIBPATH_V8"] = v8dir_tgt

  ### v8 debug
  if bld.env["USE_DEBUG"]:
    deps_tgt = join(bld.srcnode.abspath(bld.env_of_name("debug")),"deps")
    v8dir_tgt = join(deps_tgt, "v8")

    v8_debug = v8.clone("debug")
    bld.env_of_name('debug')["STATICLIB_V8"] = "v8_g"
    bld.env_of_name('debug')["LIBPATH_V8"] = v8dir_tgt
    bld.env_of_name('debug')["LINKFLAGS_V8"] = "-pthread"
    v8_debug.rule = v8rule % ( v8dir_src , deps_tgt , v8dir_tgt, "debug")
    v8_debug.target = join("deps/v8", bld.env["staticlib_PATTERN"] % "v8_g")

  ### oi
  oi = bld.new_task_gen("cc", "staticlib")
  oi.source = "deps/liboi/oi_socket.c deps/liboi/oi_buf.c"
  oi.includes = "deps/liboi/"
  oi.name = "oi"
  oi.target = "oi"
  oi.uselib = "GNUTLS"
  oi.install_path = None
  if bld.env["USE_DEBUG"]:
    oi.clone("debug")

  ### http_parser
  http_parser = bld.new_task_gen("cc", "staticlib")
  http_parser.source = "deps/http_parser/http_parser.c"
  http_parser.includes = "deps/http_parser/"
  http_parser.name = "http_parser"
  http_parser.target = "http_parser"
  http_parser.install_path = None
  if bld.env["USE_DEBUG"]:
    http_parser.clone("debug")

  ### src/native.cc
  def javascript_in_c(task):
    env = task.env
    source = map(lambda x: x.srcpath(env), task.inputs)
    targets = map(lambda x: x.srcpath(env), task.outputs)
    js2c.JS2C(source, targets)

  native_cc = bld.new_task_gen(
    source="src/http.js src/file.js src/main.js",
    target="src/natives.h",
    rule=javascript_in_c,
    before="cxx"
  )
  native_cc.install_path = None
  if bld.env["USE_DEBUG"]:
    native_cc.clone("debug")

  ### node
  node = bld.new_task_gen("cxx", "program")
  node.target = 'node'
  node.source = """
    src/node.cc
    src/http.cc
    src/net.cc
    src/file.cc
    src/timer.cc
  """
  node.includes = """
    src/ 
    deps/v8/include
    deps/libev
    deps/libeio
    deps/liboi 
    deps/http_parser
  """
  node.uselib_local = "oi ev eio http_parser"
  node.uselib = "V8 RT"
  node.install_path = '${PREFIX}/bin'
  node.chmod = 0755

  if bld.env["USE_DEBUG"]:
    node.clone("debug")

