"""SCons.Scanner.C

This module implements the depenency scanner for C/C++ code. 

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

__revision__ = "src/engine/SCons/Scanner/C.py 3842 2008/12/20 22:59:52 scons"

import SCons.Node.FS
import SCons.Scanner
import SCons.Util

import SCons.cpp

class SConsCPPScanner(SCons.cpp.PreProcessor):
    """
    SCons-specific subclass of the cpp.py module's processing.

    We subclass this so that: 1) we can deal with files represented
    by Nodes, not strings; 2) we can keep track of the files that are
    missing.
    """
    def __init__(self, *args, **kw):
        apply(SCons.cpp.PreProcessor.__init__, (self,)+args, kw)
        self.missing = []
    def initialize_result(self, fname):
        self.result = SCons.Util.UniqueList([fname])
    def finalize_result(self, fname):
        return self.result[1:]
    def find_include_file(self, t):
        keyword, quote, fname = t
        result = SCons.Node.FS.find_file(fname, self.searchpath[quote])
        if not result:
            self.missing.append((fname, self.current_file))
        return result
    def read_file(self, file):
        try:
            fp = open(str(file.rfile()))
        except EnvironmentError, e:
            self.missing.append((file, self.current_file))
            return ''
        else:
            return fp.read()

def dictify_CPPDEFINES(env):
    cppdefines = env.get('CPPDEFINES', {})
    if cppdefines is None:
        return {}
    if SCons.Util.is_Sequence(cppdefines):
        result = {}
        for c in cppdefines:
            if SCons.Util.is_Sequence(c):
                result[c[0]] = c[1]
            else:
                result[c] = None
        return result
    if not SCons.Util.is_Dict(cppdefines):
        return {cppdefines : None}
    return cppdefines

class SConsCPPScannerWrapper:
    """
    The SCons wrapper around a cpp.py scanner.

    This is the actual glue between the calling conventions of generic
    SCons scanners, and the (subclass of) cpp.py class that knows how
    to look for #include lines with reasonably real C-preprocessor-like
    evaluation of #if/#ifdef/#else/#elif lines.
    """
    def __init__(self, name, variable):
        self.name = name
        self.path = SCons.Scanner.FindPathDirs(variable)
    def __call__(self, node, env, path = ()):
        cpp = SConsCPPScanner(current = node.get_dir(),
                              cpppath = path,
                              dict = dictify_CPPDEFINES(env))
        result = cpp(node)
        for included, includer in cpp.missing:
            fmt = "No dependency generated for file: %s (included from: %s) -- file not found"
            SCons.Warnings.warn(SCons.Warnings.DependencyWarning,
                                fmt % (included, includer))
        return result

    def recurse_nodes(self, nodes):
        return nodes
    def select(self, node):
        return self

def CScanner():
    """Return a prototype Scanner instance for scanning source files
    that use the C pre-processor"""

    # Here's how we would (or might) use the CPP scanner code above that
    # knows how to evaluate #if/#ifdef/#else/#elif lines when searching
    # for #includes.  This is commented out for now until we add the
    # right configurability to let users pick between the scanners.
    #return SConsCPPScannerWrapper("CScanner", "CPPPATH")

    cs = SCons.Scanner.ClassicCPP("CScanner",
                                  "$CPPSUFFIXES",
                                  "CPPPATH",
                                  '^[ \t]*#[ \t]*(?:include|import)[ \t]*(<|")([^>"]+)(>|")')
    return cs
