#! /usr/bin/env python
import Options
import os
from os.path import join, dirname, abspath

VERSION='0.0.1'
APPNAME='node'

srcdir = '.'
blddir = 'build'

def set_options(opt):
  # the gcc module provides a --debug-level option
  opt.tool_options('compiler_cxx')
  opt.tool_options('compiler_cc')
  opt.tool_options('ragel', tdir = '.')

def configure(conf):
  conf.check_tool('compiler_cxx')
  conf.check_tool('compiler_cc')
  conf.check_tool('ragel', tooldir = '.')

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
  conf.write_config_header('config.h')


def build(bld):

  bld.add_subdirs('deps/libeio deps/libev')

  ### v8
  deps_src = join(bld.path.abspath(),"deps")
  deps_tgt = join(bld.srcnode.abspath(bld.env),"deps")
  v8dir_src = join(deps_src,"v8")
  v8dir_tgt = join(deps_tgt, "v8")
  v8lib = bld.env["staticlib_PATTERN"] % "v8"
  v8 = bld.new_task_gen(
    target=join("deps/v8",v8lib),
    rule='cp -rf %s %s && cd %s && scons library=static' 
      % ( v8dir_src
        , deps_tgt
        , v8dir_tgt
        ),
    before="cxx"
  )
  bld.env["CPPPATH_V8"] = "deps/v8/include"
  bld.env["STATICLIB_V8"] = "v8"
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

  ### node
  node = bld.new_task_gen("cxx", "program")
  node.target = 'node'
  node.source = """
    src/node.cc
    src/node_http.cc
    src/node_tcp.cc
    src/node_timer.cc
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
  node.uselib = "V8 PTHREAD"
