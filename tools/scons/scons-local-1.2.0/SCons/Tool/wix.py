"""SCons.Tool.wix

Tool-specific initialization for wix, the Windows Installer XML Tool.

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

__revision__ = "src/engine/SCons/Tool/wix.py 3842 2008/12/20 22:59:52 scons"

import SCons.Builder
import SCons.Action
import os
import string

def generate(env):
    """Add Builders and construction variables for WiX to an Environment."""
    if not exists(env):
      return

    env['WIXCANDLEFLAGS'] = ['-nologo']
    env['WIXCANDLEINCLUDE'] = []
    env['WIXCANDLECOM'] = '$WIXCANDLE $WIXCANDLEFLAGS -I $WIXCANDLEINCLUDE -o ${TARGET} ${SOURCE}'

    env['WIXLIGHTFLAGS'].append( '-nologo' )
    env['WIXLIGHTCOM'] = "$WIXLIGHT $WIXLIGHTFLAGS -out ${TARGET} ${SOURCES}"

    object_builder = SCons.Builder.Builder(
        action      = '$WIXCANDLECOM',
        suffix      = '.wxiobj',
        src_suffix  = '.wxs')

    linker_builder = SCons.Builder.Builder(
        action      = '$WIXLIGHTCOM',
        src_suffix  = '.wxiobj',
        src_builder = object_builder)

    env['BUILDERS']['WiX'] = linker_builder

def exists(env):
    env['WIXCANDLE'] = 'candle.exe'
    env['WIXLIGHT']  = 'light.exe'

    # try to find the candle.exe and light.exe tools and 
    # add the install directory to light libpath.
    #for path in os.environ['PATH'].split(os.pathsep):
    for path in string.split(os.environ['PATH'], os.pathsep):
        if not path:
            continue

        # workaround for some weird python win32 bug.
        if path[0] == '"' and path[-1:]=='"':
            path = path[1:-1]

        # normalize the path
        path = os.path.normpath(path)

        # search for the tools in the PATH environment variable
        try:
            if env['WIXCANDLE'] in os.listdir(path) and\
               env['WIXLIGHT']  in os.listdir(path):
                   env.PrependENVPath('PATH', path)
                   env['WIXLIGHTFLAGS'] = [ os.path.join( path, 'wixui.wixlib' ),
                                            '-loc',
                                            os.path.join( path, 'WixUI_en-us.wxl' ) ]
                   return 1
        except OSError:
            pass # ignore this, could be a stale PATH entry.

    return None
