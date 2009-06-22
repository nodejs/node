"""SCons.Tool.rpcgen

Tool-specific initialization for RPCGEN tools.

Three normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.
"""

#
# Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "src/engine/SCons/Tool/rpcgen.py 3842 2008/12/20 22:59:52 scons"

from SCons.Builder import Builder
import SCons.Util

cmd = "cd ${SOURCE.dir} && $RPCGEN -%s $RPCGENFLAGS %s -o ${TARGET.abspath} ${SOURCE.file}"

rpcgen_client   = cmd % ('l', '$RPCGENCLIENTFLAGS')
rpcgen_header   = cmd % ('h', '$RPCGENHEADERFLAGS')
rpcgen_service  = cmd % ('m', '$RPCGENSERVICEFLAGS')
rpcgen_xdr      = cmd % ('c', '$RPCGENXDRFLAGS')

def generate(env):
    "Add RPCGEN Builders and construction variables for an Environment."
    
    client  = Builder(action=rpcgen_client,  suffix='_clnt.c', src_suffix='.x')
    header  = Builder(action=rpcgen_header,  suffix='.h',      src_suffix='.x')
    service = Builder(action=rpcgen_service, suffix='_svc.c',  src_suffix='.x')
    xdr     = Builder(action=rpcgen_xdr,     suffix='_xdr.c',  src_suffix='.x')
    env.Append(BUILDERS={'RPCGenClient'  : client,
                         'RPCGenHeader'  : header,
                         'RPCGenService' : service,
                         'RPCGenXDR'     : xdr})
    env['RPCGEN'] = 'rpcgen'
    env['RPCGENFLAGS'] = SCons.Util.CLVar('')
    env['RPCGENCLIENTFLAGS'] = SCons.Util.CLVar('')
    env['RPCGENHEADERFLAGS'] = SCons.Util.CLVar('')
    env['RPCGENSERVICEFLAGS'] = SCons.Util.CLVar('')
    env['RPCGENXDRFLAGS'] = SCons.Util.CLVar('')

def exists(env):
    return env.Detect('rpcgen')
