"""SCons.Tool.latex

Tool-specific initialization for LaTeX.

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

__revision__ = "src/engine/SCons/Tool/latex.py 3842 2008/12/20 22:59:52 scons"

import SCons.Action
import SCons.Defaults
import SCons.Scanner.LaTeX
import SCons.Util
import SCons.Tool
import SCons.Tool.tex

LaTeXAction = None

def LaTeXAuxFunction(target = None, source= None, env=None):
    result = SCons.Tool.tex.InternalLaTeXAuxAction( LaTeXAction, target, source, env )
    return result

LaTeXAuxAction = SCons.Action.Action(LaTeXAuxFunction,
                              strfunction=SCons.Tool.tex.TeXLaTeXStrFunction)

def generate(env):
    """Add Builders and construction variables for LaTeX to an Environment."""
    global LaTeXAction
    if LaTeXAction is None:
        LaTeXAction = SCons.Action.Action('$LATEXCOM', '$LATEXCOMSTR')

    import dvi
    dvi.generate(env)

    import pdf
    pdf.generate(env)

    bld = env['BUILDERS']['DVI']
    bld.add_action('.ltx', LaTeXAuxAction)
    bld.add_action('.latex', LaTeXAuxAction)
    bld.add_emitter('.ltx', SCons.Tool.tex.tex_eps_emitter)
    bld.add_emitter('.latex', SCons.Tool.tex.tex_eps_emitter)

    env['LATEX']        = 'latex'
    env['LATEXFLAGS']   = SCons.Util.CLVar('-interaction=nonstopmode')
    env['LATEXCOM']     = 'cd ${TARGET.dir} && $LATEX $LATEXFLAGS ${SOURCE.file}'
    env['LATEXRETRIES'] = 3

def exists(env):
    return env.Detect('latex')
