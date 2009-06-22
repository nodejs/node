"""SCons.Tool.pdftex

Tool-specific initialization for pdftex.

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

__revision__ = "src/engine/SCons/Tool/pdftex.py 3842 2008/12/20 22:59:52 scons"

import SCons.Action
import SCons.Util
import SCons.Tool.tex

PDFTeXAction = None

# This action might be needed more than once if we are dealing with
# labels and bibtex.
PDFLaTeXAction = None

def PDFLaTeXAuxAction(target = None, source= None, env=None):
    result = SCons.Tool.tex.InternalLaTeXAuxAction( PDFLaTeXAction, target, source, env )
    return result

def PDFTeXLaTeXFunction(target = None, source= None, env=None):
    """A builder for TeX and LaTeX that scans the source file to
    decide the "flavor" of the source and then executes the appropriate
    program."""
    if SCons.Tool.tex.is_LaTeX(source):
        result = PDFLaTeXAuxAction(target,source,env)
    else:
        result = PDFTeXAction(target,source,env)
    return result

PDFTeXLaTeXAction = None

def generate(env):
    """Add Builders and construction variables for pdftex to an Environment."""
    global PDFTeXAction
    if PDFTeXAction is None:
        PDFTeXAction = SCons.Action.Action('$PDFTEXCOM', '$PDFTEXCOMSTR')

    global PDFLaTeXAction
    if PDFLaTeXAction is None:
        PDFLaTeXAction = SCons.Action.Action("$PDFLATEXCOM", "$PDFLATEXCOMSTR")

    global PDFTeXLaTeXAction
    if PDFTeXLaTeXAction is None:
        PDFTeXLaTeXAction = SCons.Action.Action(PDFTeXLaTeXFunction,
                              strfunction=SCons.Tool.tex.TeXLaTeXStrFunction)

    import pdf
    pdf.generate(env)

    bld = env['BUILDERS']['PDF']
    bld.add_action('.tex', PDFTeXLaTeXAction)
    bld.add_emitter('.tex', SCons.Tool.tex.tex_pdf_emitter)

    # Add the epstopdf builder after the pdftex builder 
    # so pdftex is the default for no source suffix
    pdf.generate2(env)

    env['PDFTEX']      = 'pdftex'
    env['PDFTEXFLAGS'] = SCons.Util.CLVar('-interaction=nonstopmode')
    env['PDFTEXCOM']   = 'cd ${TARGET.dir} && $PDFTEX $PDFTEXFLAGS ${SOURCE.file}'

    # Duplicate from latex.py.  If latex.py goes away, then this is still OK.
    env['PDFLATEX']      = 'pdflatex'
    env['PDFLATEXFLAGS'] = SCons.Util.CLVar('-interaction=nonstopmode')
    env['PDFLATEXCOM']   = 'cd ${TARGET.dir} && $PDFLATEX $PDFLATEXFLAGS ${SOURCE.file}'
    env['LATEXRETRIES']  = 3

def exists(env):
    return env.Detect('pdftex')
