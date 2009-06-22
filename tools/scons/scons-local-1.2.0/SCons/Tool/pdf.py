"""SCons.Tool.pdf

Common PDF Builder definition for various other Tool modules that use it.
Add an explicit action to run epstopdf to convert .eps files to .pdf

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

__revision__ = "src/engine/SCons/Tool/pdf.py 3842 2008/12/20 22:59:52 scons"

import SCons.Builder
import SCons.Tool

PDFBuilder = None

EpsPdfAction = SCons.Action.Action('$EPSTOPDFCOM', '$EPSTOPDFCOMSTR')

def generate(env):
    try:
        env['BUILDERS']['PDF']
    except KeyError:
        global PDFBuilder
        if PDFBuilder is None:
            PDFBuilder = SCons.Builder.Builder(action = {},
                                               source_scanner = SCons.Tool.PDFLaTeXScanner,
                                               prefix = '$PDFPREFIX',
                                               suffix = '$PDFSUFFIX',
                                               emitter = {},
                                               source_ext_match = None,
                                               single_source=True)
        env['BUILDERS']['PDF'] = PDFBuilder

    env['PDFPREFIX'] = ''
    env['PDFSUFFIX'] = '.pdf'

# put the epstopdf builder in this routine so we can add it after 
# the pdftex builder so that one is the default for no source suffix
def generate2(env):
    bld = env['BUILDERS']['PDF']
    #bld.add_action('.ps',  EpsPdfAction) # this is covered by direct Ghostcript action in gs.py
    bld.add_action('.eps', EpsPdfAction)

    env['EPSTOPDF']      = 'epstopdf'
    env['EPSTOPDFFLAGS'] = SCons.Util.CLVar('')
    env['EPSTOPDFCOM']   = '$EPSTOPDF $EPSTOPDFFLAGS ${SOURCE} -o ${TARGET}'

def exists(env):
    # This only puts a skeleton Builder in place, so if someone
    # references this Tool directly, it's always "available."
    return 1
