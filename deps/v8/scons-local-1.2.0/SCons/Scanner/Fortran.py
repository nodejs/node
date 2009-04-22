"""SCons.Scanner.Fortran

This module implements the dependency scanner for Fortran code.

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

__revision__ = "src/engine/SCons/Scanner/Fortran.py 3842 2008/12/20 22:59:52 scons"

import re
import string

import SCons.Node
import SCons.Node.FS
import SCons.Scanner
import SCons.Util
import SCons.Warnings

class F90Scanner(SCons.Scanner.Classic):
    """
    A Classic Scanner subclass for Fortran source files which takes
    into account both USE and INCLUDE statements.  This scanner will
    work for both F77 and F90 (and beyond) compilers.

    Currently, this scanner assumes that the include files do not contain
    USE statements.  To enable the ability to deal with USE statements
    in include files, add logic right after the module names are found
    to loop over each include file, search for and locate each USE
    statement, and append each module name to the list of dependencies.
    Caching the search results in a common dictionary somewhere so that
    the same include file is not searched multiple times would be a
    smart thing to do.
    """

    def __init__(self, name, suffixes, path_variable,
                 use_regex, incl_regex, def_regex, *args, **kw):

        self.cre_use = re.compile(use_regex, re.M)
        self.cre_incl = re.compile(incl_regex, re.M)
        self.cre_def = re.compile(def_regex, re.M)

        def _scan(node, env, path, self=self):
            node = node.rfile()

            if not node.exists():
                return []

            return self.scan(node, env, path)

        kw['function'] = _scan
        kw['path_function'] = SCons.Scanner.FindPathDirs(path_variable)
        kw['recursive'] = 1
        kw['skeys'] = suffixes
        kw['name'] = name

        apply(SCons.Scanner.Current.__init__, (self,) + args, kw)

    def scan(self, node, env, path=()):

        # cache the includes list in node so we only scan it once:
        if node.includes != None:
            mods_and_includes = node.includes
        else:
            # retrieve all included filenames
            includes = self.cre_incl.findall(node.get_contents())
            # retrieve all USE'd module names
            modules = self.cre_use.findall(node.get_contents())
            # retrieve all defined module names
            defmodules = self.cre_def.findall(node.get_contents())

            # Remove all USE'd module names that are defined in the same file
            d = {}
            for m in defmodules:
                d[m] = 1
            modules = filter(lambda m, d=d: not d.has_key(m), modules)
            #modules = self.undefinedModules(modules, defmodules)

            # Convert module name to a .mod filename
            suffix = env.subst('$FORTRANMODSUFFIX')
            modules = map(lambda x, s=suffix: string.lower(x) + s, modules)
            # Remove unique items from the list
            mods_and_includes = SCons.Util.unique(includes+modules)
            node.includes = mods_and_includes

        # This is a hand-coded DSU (decorate-sort-undecorate, or
        # Schwartzian transform) pattern.  The sort key is the raw name
        # of the file as specifed on the USE or INCLUDE line, which lets
        # us keep the sort order constant regardless of whether the file
        # is actually found in a Repository or locally.
        nodes = []
        source_dir = node.get_dir()
        if callable(path):
            path = path()
        for dep in mods_and_includes:
            n, i = self.find_include(dep, source_dir, path)

            if n is None:
                SCons.Warnings.warn(SCons.Warnings.DependencyWarning,
                                    "No dependency generated for file: %s (referenced by: %s) -- file not found" % (i, node))
            else:
                sortkey = self.sort_key(dep)
                nodes.append((sortkey, n))

        nodes.sort()
        nodes = map(lambda pair: pair[1], nodes)
        return nodes

def FortranScan(path_variable="FORTRANPATH"):
    """Return a prototype Scanner instance for scanning source files
    for Fortran USE & INCLUDE statements"""

#   The USE statement regex matches the following:
#
#   USE module_name
#   USE :: module_name
#   USE, INTRINSIC :: module_name
#   USE, NON_INTRINSIC :: module_name
#
#   Limitations
#
#   --  While the regex can handle multiple USE statements on one line,
#       it cannot properly handle them if they are commented out.
#       In either of the following cases:
#
#            !  USE mod_a ; USE mod_b         [entire line is commented out]
#               USE mod_a ! ; USE mod_b       [in-line comment of second USE statement]
#
#       the second module name (mod_b) will be picked up as a dependency
#       even though it should be ignored.  The only way I can see
#       to rectify this would be to modify the scanner to eliminate
#       the call to re.findall, read in the contents of the file,
#       treating the comment character as an end-of-line character
#       in addition to the normal linefeed, loop over each line,
#       weeding out the comments, and looking for the USE statements.
#       One advantage to this is that the regex passed to the scanner
#       would no longer need to match a semicolon.
#
#   --  I question whether or not we need to detect dependencies to
#       INTRINSIC modules because these are built-in to the compiler.
#       If we consider them a dependency, will SCons look for them, not
#       find them, and kill the build?  Or will we there be standard
#       compiler-specific directories we will need to point to so the
#       compiler and SCons can locate the proper object and mod files?

#   Here is a breakdown of the regex:
#
#   (?i)               : regex is case insensitive
#   ^                  : start of line
#   (?:                : group a collection of regex symbols without saving the match as a "group"
#      ^|;             : matches either the start of the line or a semicolon - semicolon
#   )                  : end the unsaved grouping
#   \s*                : any amount of white space
#   USE                : match the string USE, case insensitive
#   (?:                : group a collection of regex symbols without saving the match as a "group"
#      \s+|            : match one or more whitespace OR ....  (the next entire grouped set of regex symbols)
#      (?:             : group a collection of regex symbols without saving the match as a "group"
#         (?:          : establish another unsaved grouping of regex symbols
#            \s*          : any amount of white space
#            ,         : match a comma
#            \s*       : any amount of white space
#            (?:NON_)? : optionally match the prefix NON_, case insensitive
#            INTRINSIC : match the string INTRINSIC, case insensitive
#         )?           : optionally match the ", INTRINSIC/NON_INTRINSIC" grouped expression
#         \s*          : any amount of white space
#         ::           : match a double colon that must appear after the INTRINSIC/NON_INTRINSIC attribute
#      )               : end the unsaved grouping
#   )                  : end the unsaved grouping
#   \s*                : match any amount of white space
#   (\w+)              : match the module name that is being USE'd
#
#
    use_regex = "(?i)(?:^|;)\s*USE(?:\s+|(?:(?:\s*,\s*(?:NON_)?INTRINSIC)?\s*::))\s*(\w+)"


#   The INCLUDE statement regex matches the following:
#
#   INCLUDE 'some_Text'
#   INCLUDE "some_Text"
#   INCLUDE "some_Text" ; INCLUDE "some_Text"
#   INCLUDE kind_"some_Text"
#   INCLUDE kind_'some_Text"
#
#   where some_Text can include any alphanumeric and/or special character
#   as defined by the Fortran 2003 standard.
#
#   Limitations:
#
#   --  The Fortran standard dictates that a " or ' in the INCLUDE'd
#       string must be represented as a "" or '', if the quotes that wrap
#       the entire string are either a ' or ", respectively.   While the
#       regular expression below can detect the ' or " characters just fine,
#       the scanning logic, presently is unable to detect them and reduce
#       them to a single instance.  This probably isn't an issue since,
#       in practice, ' or " are not generally used in filenames.
#
#   --  This regex will not properly deal with multiple INCLUDE statements
#       when the entire line has been commented out, ala
#
#           ! INCLUDE 'some_file' ; INCLUDE 'some_file'
#
#       In such cases, it will properly ignore the first INCLUDE file,
#       but will actually still pick up the second.  Interestingly enough,
#       the regex will properly deal with these cases:
#
#             INCLUDE 'some_file'
#             INCLUDE 'some_file' !; INCLUDE 'some_file'
#
#       To get around the above limitation, the FORTRAN programmer could
#       simply comment each INCLUDE statement separately, like this
#
#           ! INCLUDE 'some_file' !; INCLUDE 'some_file'
#
#       The way I see it, the only way to get around this limitation would
#       be to modify the scanning logic to replace the calls to re.findall
#       with a custom loop that processes each line separately, throwing
#       away fully commented out lines before attempting to match against
#       the INCLUDE syntax.
#
#   Here is a breakdown of the regex:
#
#   (?i)               : regex is case insensitive
#   (?:                : begin a non-saving group that matches the following:
#      ^               :    either the start of the line
#      |               :                or
#      ['">]\s*;       :    a semicolon that follows a single quote,
#                           double quote or greater than symbol (with any
#                           amount of whitespace in between).  This will
#                           allow the regex to match multiple INCLUDE
#                           statements per line (although it also requires
#                           the positive lookahead assertion that is
#                           used below).  It will even properly deal with
#                           (i.e. ignore) cases in which the additional
#                           INCLUDES are part of an in-line comment, ala
#                                           "  INCLUDE 'someFile' ! ; INCLUDE 'someFile2' "
#   )                  : end of non-saving group
#   \s*                : any amount of white space
#   INCLUDE            : match the string INCLUDE, case insensitive
#   \s+                : match one or more white space characters
#   (?\w+_)?           : match the optional "kind-param _" prefix allowed by the standard
#   [<"']              : match the include delimiter - an apostrophe, double quote, or less than symbol
#   (.+?)              : match one or more characters that make up
#                        the included path and file name and save it
#                        in a group.  The Fortran standard allows for
#                        any non-control character to be used.  The dot
#                        operator will pick up any character, including
#                        control codes, but I can't conceive of anyone
#                        putting control codes in their file names.
#                        The question mark indicates it is non-greedy so
#                        that regex will match only up to the next quote,
#                        double quote, or greater than symbol
#   (?=["'>])          : positive lookahead assertion to match the include
#                        delimiter - an apostrophe, double quote, or
#                        greater than symbol.  This level of complexity
#                        is required so that the include delimiter is
#                        not consumed by the match, thus allowing the
#                        sub-regex discussed above to uniquely match a
#                        set of semicolon-separated INCLUDE statements
#                        (as allowed by the F2003 standard)

    include_regex = """(?i)(?:^|['">]\s*;)\s*INCLUDE\s+(?:\w+_)?[<"'](.+?)(?=["'>])"""

#   The MODULE statement regex finds module definitions by matching
#   the following:
#
#   MODULE module_name
#
#   but *not* the following:
#
#   MODULE PROCEDURE procedure_name
#
#   Here is a breakdown of the regex:
#
#   (?i)               : regex is case insensitive
#   ^\s*               : any amount of white space
#   MODULE             : match the string MODULE, case insensitive
#   \s+                : match one or more white space characters
#   (?!PROCEDURE)      : but *don't* match if the next word matches
#                        PROCEDURE (negative lookahead assertion),
#                        case insensitive
#   (\w+)              : match one or more alphanumeric characters
#                        that make up the defined module name and
#                        save it in a group

    def_regex = """(?i)^\s*MODULE\s+(?!PROCEDURE)(\w+)"""

    scanner = F90Scanner("FortranScan",
                         "$FORTRANSUFFIXES",
                         path_variable,
                         use_regex,
                         include_regex,
                         def_regex)
    return scanner
