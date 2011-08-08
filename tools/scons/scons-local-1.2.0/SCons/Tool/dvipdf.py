"""SCons.Tool.dvipdf

Tool-specific initialization for dvipdf.

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

__revision__ = "src/engine/SCons/Tool/dvipdf.py 3842 2008/12/20 22:59:52 scons"

import SCons.Action
import SCons.Defaults
import SCons.Tool.pdf
import SCons.Tool.tex
import SCons.Util

_null = SCons.Scanner.LaTeX._null

def DviPdfPsFunction(XXXDviAction, target = None, source= None, env=None):
    """A builder for DVI files that sets the TEXPICTS environment
       variable before running dvi2ps or dvipdf."""

    try:
        abspath = source[0].attributes.path
    except AttributeError :
        abspath =  ''

    saved_env = SCons.Scanner.LaTeX.modify_env_var(env, 'TEXPICTS', abspath)

    result = XXXDviAction(target, source, env)

    if saved_env is _null:
        try:
            del env['ENV']['TEXPICTS']
        except KeyError:
            pass # was never set
    else:
        env['ENV']['TEXPICTS'] = saved_env

    return result

def DviPdfFunction(target = None, source= None, env=None):
    result = DviPdfPsFunction(PDFAction,target,source,env)
    return result

def DviPdfStrFunction(target = None, source= None, env=None):
    """A strfunction for dvipdf that returns the appropriate
    command string for the no_exec options."""
    if env.GetOption("no_exec"):
        result = env.subst('$DVIPDFCOM',0,target,source)
    else:
        result = ''
    return result

PDFAction = None
DVIPDFAction = None

def PDFEmitter(target, source, env):
    """Strips any .aux or .log files from the input source list.
    These are created by the TeX Builder that in all likelihood was
    used to generate the .dvi file we're using as input, and we only
    care about the .dvi file.
    """
    def strip_suffixes(n):
        return not SCons.Util.splitext(str(n))[1] in ['.aux', '.log']
    source = filter(strip_suffixes, source)
    return (target, source)

def generate(env):
    """Add Builders and construction variables for dvipdf to an Environment."""
    global PDFAction
    if PDFAction is None:
        PDFAction = SCons.Action.Action('$DVIPDFCOM', '$DVIPDFCOMSTR')

    global DVIPDFAction
    if DVIPDFAction is None:
        DVIPDFAction = SCons.Action.Action(DviPdfFunction, strfunction = DviPdfStrFunction)

    import pdf
    pdf.generate(env)

    bld = env['BUILDERS']['PDF']
    bld.add_action('.dvi', DVIPDFAction)
    bld.add_emitter('.dvi', PDFEmitter)

    env['DVIPDF']      = 'dvipdf'
    env['DVIPDFFLAGS'] = SCons.Util.CLVar('')
    env['DVIPDFCOM']   = 'cd ${TARGET.dir} && $DVIPDF $DVIPDFFLAGS ${SOURCE.file} ${TARGET.file}'

    # Deprecated synonym.
    env['PDFCOM']      = ['$DVIPDFCOM']

def exists(env):
    return env.Detect('dvipdf')
