"""SCons.Tool.nasm

Tool-specific initialization for nasm, the famous Netwide Assembler.

There normally shouldn't be any need to import this module directly.
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

__revision__ = "src/engine/SCons/Tool/nasm.py 3842 2008/12/20 22:59:52 scons"

import SCons.Defaults
import SCons.Tool
import SCons.Util

ASSuffixes = ['.s', '.asm', '.ASM']
ASPPSuffixes = ['.spp', '.SPP', '.sx']
if SCons.Util.case_sensitive_suffixes('.s', '.S'):
    ASPPSuffixes.extend(['.S'])
else:
    ASSuffixes.extend(['.S'])

def generate(env):
    """Add Builders and construction variables for nasm to an Environment."""
    static_obj, shared_obj = SCons.Tool.createObjBuilders(env)

    for suffix in ASSuffixes:
        static_obj.add_action(suffix, SCons.Defaults.ASAction)
        static_obj.add_emitter(suffix, SCons.Defaults.StaticObjectEmitter)

    for suffix in ASPPSuffixes:
        static_obj.add_action(suffix, SCons.Defaults.ASPPAction)
        static_obj.add_emitter(suffix, SCons.Defaults.StaticObjectEmitter)

    env['AS']        = 'nasm'
    env['ASFLAGS']   = SCons.Util.CLVar('')
    env['ASPPFLAGS'] = '$ASFLAGS'
    env['ASCOM']     = '$AS $ASFLAGS -o $TARGET $SOURCES'
    env['ASPPCOM']   = '$CC $ASPPFLAGS $CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS -c -o $TARGET $SOURCES'

def exists(env):
    return env.Detect('nasm')
