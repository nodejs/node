"""SCons.Conftest

Autoconf-like configuration support; low level implementation of tests.
"""

#
# Copyright (c) 2003 Stichting NLnet Labs
# Copyright (c) 2001, 2002, 2003 Steven Knight
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

#
# The purpose of this module is to define how a check is to be performed.
# Use one of the Check...() functions below.
#

#
# A context class is used that defines functions for carrying out the tests,
# logging and messages.  The following methods and members must be present:
#
# context.Display(msg)  Function called to print messages that are normally
#                       displayed for the user.  Newlines are explicitly used.
#                       The text should also be written to the logfile!
#
# context.Log(msg)      Function called to write to a log file.
#
# context.BuildProg(text, ext)
#                       Function called to build a program, using "ext" for the
#                       file extention.  Must return an empty string for
#                       success, an error message for failure.
#                       For reliable test results building should be done just
#                       like an actual program would be build, using the same
#                       command and arguments (including configure results so
#                       far).
#
# context.CompileProg(text, ext)
#                       Function called to compile a program, using "ext" for
#                       the file extention.  Must return an empty string for
#                       success, an error message for failure.
#                       For reliable test results compiling should be done just
#                       like an actual source file would be compiled, using the
#                       same command and arguments (including configure results
#                       so far).
#
# context.AppendLIBS(lib_name_list)
#                       Append "lib_name_list" to the value of LIBS.
#                       "lib_namelist" is a list of strings.
#                       Return the value of LIBS before changing it (any type
#                       can be used, it is passed to SetLIBS() later.
#
# context.SetLIBS(value)
#                       Set LIBS to "value".  The type of "value" is what
#                       AppendLIBS() returned.
#                       Return the value of LIBS before changing it (any type
#                       can be used, it is passed to SetLIBS() later.
#
# context.headerfilename
#                       Name of file to append configure results to, usually
#                       "confdefs.h".
#                       The file must not exist or be empty when starting.
#                       Empty or None to skip this (some tests will not work!).
#
# context.config_h      (may be missing). If present, must be a string, which
#                       will be filled with the contents of a config_h file.
#
# context.vardict       Dictionary holding variables used for the tests and
#                       stores results from the tests, used for the build
#                       commands.
#                       Normally contains "CC", "LIBS", "CPPFLAGS", etc.
#
# context.havedict      Dictionary holding results from the tests that are to
#                       be used inside a program.
#                       Names often start with "HAVE_".  These are zero
#                       (feature not present) or one (feature present).  Other
#                       variables may have any value, e.g., "PERLVERSION" can
#                       be a number and "SYSTEMNAME" a string.
#

import re
import string
from types import IntType

#
# PUBLIC VARIABLES
#

LogInputFiles = 1    # Set that to log the input files in case of a failed test
LogErrorMessages = 1 # Set that to log Conftest-generated error messages

#
# PUBLIC FUNCTIONS
#

# Generic remarks:
# - When a language is specified which is not supported the test fails.  The
#   message is a bit different, because not all the arguments for the normal
#   message are available yet (chicken-egg problem).


def CheckBuilder(context, text = None, language = None):
    """
    Configure check to see if the compiler works.
    Note that this uses the current value of compiler and linker flags, make
    sure $CFLAGS, $CPPFLAGS and $LIBS are set correctly.
    "language" should be "C" or "C++" and is used to select the compiler.
    Default is "C".
    "text" may be used to specify the code to be build.
    Returns an empty string for success, an error message for failure.
    """
    lang, suffix, msg = _lang2suffix(language)
    if msg:
        context.Display("%s\n" % msg)
        return msg

    if not text:
        text = """
int main() {
    return 0;
}
"""

    context.Display("Checking if building a %s file works... " % lang)
    ret = context.BuildProg(text, suffix)
    _YesNoResult(context, ret, None, text)
    return ret

def CheckCC(context):
    """
    Configure check for a working C compiler.

    This checks whether the C compiler, as defined in the $CC construction
    variable, can compile a C source file. It uses the current $CCCOM value
    too, so that it can test against non working flags.

    """
    context.Display("Checking whether the C compiler works")
    text = """
int main()
{
    return 0;
}
"""
    ret = _check_empty_program(context, 'CC', text, 'C')
    _YesNoResult(context, ret, None, text)
    return ret

def CheckSHCC(context):
    """
    Configure check for a working shared C compiler.

    This checks whether the C compiler, as defined in the $SHCC construction
    variable, can compile a C source file. It uses the current $SHCCCOM value
    too, so that it can test against non working flags.

    """
    context.Display("Checking whether the (shared) C compiler works")
    text = """
int foo()
{
    return 0;
}
"""
    ret = _check_empty_program(context, 'SHCC', text, 'C', use_shared = True)
    _YesNoResult(context, ret, None, text)
    return ret

def CheckCXX(context):
    """
    Configure check for a working CXX compiler.

    This checks whether the CXX compiler, as defined in the $CXX construction
    variable, can compile a CXX source file. It uses the current $CXXCOM value
    too, so that it can test against non working flags.

    """
    context.Display("Checking whether the C++ compiler works")
    text = """
int main()
{
    return 0;
}
"""
    ret = _check_empty_program(context, 'CXX', text, 'C++')
    _YesNoResult(context, ret, None, text)
    return ret

def CheckSHCXX(context):
    """
    Configure check for a working shared CXX compiler.

    This checks whether the CXX compiler, as defined in the $SHCXX construction
    variable, can compile a CXX source file. It uses the current $SHCXXCOM value
    too, so that it can test against non working flags.

    """
    context.Display("Checking whether the (shared) C++ compiler works")
    text = """
int main()
{
    return 0;
}
"""
    ret = _check_empty_program(context, 'SHCXX', text, 'C++', use_shared = True)
    _YesNoResult(context, ret, None, text)
    return ret

def _check_empty_program(context, comp, text, language, use_shared = False):
    """Return 0 on success, 1 otherwise."""
    if not context.env.has_key(comp) or not context.env[comp]:
        # The compiler construction variable is not set or empty
        return 1

    lang, suffix, msg = _lang2suffix(language)
    if msg:
        return 1

    if use_shared:
        return context.CompileSharedObject(text, suffix)
    else:
        return context.CompileProg(text, suffix)


def CheckFunc(context, function_name, header = None, language = None):
    """
    Configure check for a function "function_name".
    "language" should be "C" or "C++" and is used to select the compiler.
    Default is "C".
    Optional "header" can be defined to define a function prototype, include a
    header file or anything else that comes before main().
    Sets HAVE_function_name in context.havedict according to the result.
    Note that this uses the current value of compiler and linker flags, make
    sure $CFLAGS, $CPPFLAGS and $LIBS are set correctly.
    Returns an empty string for success, an error message for failure.
    """

    # Remarks from autoconf:
    # - Don't include <ctype.h> because on OSF/1 3.0 it includes <sys/types.h>
    #   which includes <sys/select.h> which contains a prototype for select.
    #   Similarly for bzero.
    # - assert.h is included to define __stub macros and hopefully few
    #   prototypes, which can conflict with char $1(); below.
    # - Override any gcc2 internal prototype to avoid an error.
    # - We use char for the function declaration because int might match the
    #   return type of a gcc2 builtin and then its argument prototype would
    #   still apply.
    # - The GNU C library defines this for functions which it implements to
    #   always fail with ENOSYS.  Some functions are actually named something
    #   starting with __ and the normal name is an alias.

    if context.headerfilename:
        includetext = '#include "%s"' % context.headerfilename
    else:
        includetext = ''
    if not header:
        header = """
#ifdef __cplusplus
extern "C"
#endif
char %s();""" % function_name

    lang, suffix, msg = _lang2suffix(language)
    if msg:
        context.Display("Cannot check for %s(): %s\n" % (function_name, msg))
        return msg

    text = """
%(include)s
#include <assert.h>
%(hdr)s

int main() {
#if defined (__stub_%(name)s) || defined (__stub___%(name)s)
  fail fail fail
#else
  %(name)s();
#endif

  return 0;
}
""" % { 'name': function_name,
        'include': includetext,
        'hdr': header }

    context.Display("Checking for %s function %s()... " % (lang, function_name))
    ret = context.BuildProg(text, suffix)
    _YesNoResult(context, ret, "HAVE_" + function_name, text,
                 "Define to 1 if the system has the function `%s'." %\
                 function_name)
    return ret


def CheckHeader(context, header_name, header = None, language = None,
                                                        include_quotes = None):
    """
    Configure check for a C or C++ header file "header_name".
    Optional "header" can be defined to do something before including the
    header file (unusual, supported for consistency).
    "language" should be "C" or "C++" and is used to select the compiler.
    Default is "C".
    Sets HAVE_header_name in context.havedict according to the result.
    Note that this uses the current value of compiler and linker flags, make
    sure $CFLAGS and $CPPFLAGS are set correctly.
    Returns an empty string for success, an error message for failure.
    """
    # Why compile the program instead of just running the preprocessor?
    # It is possible that the header file exists, but actually using it may
    # fail (e.g., because it depends on other header files).  Thus this test is
    # more strict.  It may require using the "header" argument.
    #
    # Use <> by default, because the check is normally used for system header
    # files.  SCons passes '""' to overrule this.

    # Include "confdefs.h" first, so that the header can use HAVE_HEADER_H.
    if context.headerfilename:
        includetext = '#include "%s"\n' % context.headerfilename
    else:
        includetext = ''
    if not header:
        header = ""

    lang, suffix, msg = _lang2suffix(language)
    if msg:
        context.Display("Cannot check for header file %s: %s\n"
                                                          % (header_name, msg))
        return msg

    if not include_quotes:
        include_quotes = "<>"

    text = "%s%s\n#include %s%s%s\n\n" % (includetext, header,
                             include_quotes[0], header_name, include_quotes[1])

    context.Display("Checking for %s header file %s... " % (lang, header_name))
    ret = context.CompileProg(text, suffix)
    _YesNoResult(context, ret, "HAVE_" + header_name, text, 
                 "Define to 1 if you have the <%s> header file." % header_name)
    return ret


def CheckType(context, type_name, fallback = None,
                                               header = None, language = None):
    """
    Configure check for a C or C++ type "type_name".
    Optional "header" can be defined to include a header file.
    "language" should be "C" or "C++" and is used to select the compiler.
    Default is "C".
    Sets HAVE_type_name in context.havedict according to the result.
    Note that this uses the current value of compiler and linker flags, make
    sure $CFLAGS, $CPPFLAGS and $LIBS are set correctly.
    Returns an empty string for success, an error message for failure.
    """

    # Include "confdefs.h" first, so that the header can use HAVE_HEADER_H.
    if context.headerfilename:
        includetext = '#include "%s"' % context.headerfilename
    else:
        includetext = ''
    if not header:
        header = ""

    lang, suffix, msg = _lang2suffix(language)
    if msg:
        context.Display("Cannot check for %s type: %s\n" % (type_name, msg))
        return msg

    # Remarks from autoconf about this test:
    # - Grepping for the type in include files is not reliable (grep isn't
    #   portable anyway).
    # - Using "TYPE my_var;" doesn't work for const qualified types in C++.
    #   Adding an initializer is not valid for some C++ classes.
    # - Using the type as parameter to a function either fails for K&$ C or for
    #   C++.
    # - Using "TYPE *my_var;" is valid in C for some types that are not
    #   declared (struct something).
    # - Using "sizeof(TYPE)" is valid when TYPE is actually a variable.
    # - Using the previous two together works reliably.
    text = """
%(include)s
%(header)s

int main() {
  if ((%(name)s *) 0)
    return 0;
  if (sizeof (%(name)s))
    return 0;
}
""" % { 'include': includetext,
        'header': header,
        'name': type_name }

    context.Display("Checking for %s type %s... " % (lang, type_name))
    ret = context.BuildProg(text, suffix)
    _YesNoResult(context, ret, "HAVE_" + type_name, text,
                 "Define to 1 if the system has the type `%s'." % type_name)
    if ret and fallback and context.headerfilename:
        f = open(context.headerfilename, "a")
        f.write("typedef %s %s;\n" % (fallback, type_name))
        f.close()

    return ret

def CheckTypeSize(context, type_name, header = None, language = None, expect = None):
    """This check can be used to get the size of a given type, or to check whether
    the type is of expected size.

    Arguments:
        - type : str
            the type to check
        - includes : sequence
            list of headers to include in the test code before testing the type
        - language : str
            'C' or 'C++'
        - expect : int
            if given, will test wether the type has the given number of bytes.
            If not given, will automatically find the size.

        Returns:
            status : int
                0 if the check failed, or the found size of the type if the check succeeded."""
    
    # Include "confdefs.h" first, so that the header can use HAVE_HEADER_H.
    if context.headerfilename:
        includetext = '#include "%s"' % context.headerfilename
    else:
        includetext = ''

    if not header:
        header = ""

    lang, suffix, msg = _lang2suffix(language)
    if msg:
        context.Display("Cannot check for %s type: %s\n" % (type_name, msg))
        return msg

    src = includetext + header 
    if not expect is None:
        # Only check if the given size is the right one
        context.Display('Checking %s is %d bytes... ' % (type_name, expect))

        # test code taken from autoconf: this is a pretty clever hack to find that
        # a type is of a given size using only compilation. This speeds things up
        # quite a bit compared to straightforward code using TryRun
        src = src + r"""
typedef %s scons_check_type;

int main()
{
    static int test_array[1 - 2 * !(((long int) (sizeof(scons_check_type))) == %d)];
    test_array[0] = 0;

    return 0;
}
"""

        st = context.CompileProg(src % (type_name, expect), suffix)
        if not st:
            context.Display("yes\n")
            _Have(context, "SIZEOF_%s" % type_name, expect, 
                  "The size of `%s', as computed by sizeof." % type_name)
            return expect
        else:
            context.Display("no\n")
            _LogFailed(context, src, st)
            return 0
    else:
        # Only check if the given size is the right one
        context.Message('Checking size of %s ... ' % type_name)

        # We have to be careful with the program we wish to test here since
        # compilation will be attempted using the current environment's flags.
        # So make sure that the program will compile without any warning. For
        # example using: 'int main(int argc, char** argv)' will fail with the
        # '-Wall -Werror' flags since the variables argc and argv would not be
        # used in the program...
        #
        src = src + """
#include <stdlib.h>
#include <stdio.h>
int main() {
    printf("%d", (int)sizeof(""" + type_name + """));
    return 0;
}
    """
        st, out = context.RunProg(src, suffix)
        try:
            size = int(out)
        except ValueError:
            # If cannot convert output of test prog to an integer (the size),
            # something went wront, so just fail
            st = 1
            size = 0

        if not st:
            context.Display("yes\n")
            _Have(context, "SIZEOF_%s" % type_name, size,
                  "The size of `%s', as computed by sizeof." % type_name)
            return size
        else:
            context.Display("no\n")
            _LogFailed(context, src, st)
            return 0

    return 0

def CheckDeclaration(context, symbol, includes = None, language = None):
    """Checks whether symbol is declared.

    Use the same test as autoconf, that is test whether the symbol is defined
    as a macro or can be used as an r-value.

    Arguments:
        symbol : str
            the symbol to check
        includes : str
            Optional "header" can be defined to include a header file.
        language : str
            only C and C++ supported.

    Returns:
        status : bool
            True if the check failed, False if succeeded."""
    
    # Include "confdefs.h" first, so that the header can use HAVE_HEADER_H.
    if context.headerfilename:
        includetext = '#include "%s"' % context.headerfilename
    else:
        includetext = ''

    if not includes:
        includes = ""

    lang, suffix, msg = _lang2suffix(language)
    if msg:
        context.Display("Cannot check for declaration %s: %s\n" % (type_name, msg))
        return msg

    src = includetext + includes 
    context.Display('Checking whether %s is declared... ' % symbol)

    src = src + r"""
int main()
{
#ifndef %s
    (void) %s;
#endif
    ;
    return 0;
}
""" % (symbol, symbol)

    st = context.CompileProg(src, suffix)
    _YesNoResult(context, st, "HAVE_DECL_" + symbol, src,
                 "Set to 1 if %s is defined." % symbol)
    return st

def CheckLib(context, libs, func_name = None, header = None,
                 extra_libs = None, call = None, language = None, autoadd = 1):
    """
    Configure check for a C or C++ libraries "libs".  Searches through
    the list of libraries, until one is found where the test succeeds.
    Tests if "func_name" or "call" exists in the library.  Note: if it exists
    in another library the test succeeds anyway!
    Optional "header" can be defined to include a header file.  If not given a
    default prototype for "func_name" is added.
    Optional "extra_libs" is a list of library names to be added after
    "lib_name" in the build command.  To be used for libraries that "lib_name"
    depends on.
    Optional "call" replaces the call to "func_name" in the test code.  It must
    consist of complete C statements, including a trailing ";".
    Both "func_name" and "call" arguments are optional, and in that case, just
    linking against the libs is tested.
    "language" should be "C" or "C++" and is used to select the compiler.
    Default is "C".
    Note that this uses the current value of compiler and linker flags, make
    sure $CFLAGS, $CPPFLAGS and $LIBS are set correctly.
    Returns an empty string for success, an error message for failure.
    """
    # Include "confdefs.h" first, so that the header can use HAVE_HEADER_H.
    if context.headerfilename:
        includetext = '#include "%s"' % context.headerfilename
    else:
        includetext = ''
    if not header:
        header = ""

    text = """
%s
%s""" % (includetext, header)

    # Add a function declaration if needed.
    if func_name and func_name != "main":
        if not header:
            text = text + """
#ifdef __cplusplus
extern "C"
#endif
char %s();
""" % func_name

        # The actual test code.
        if not call:
            call = "%s();" % func_name

    # if no function to test, leave main() blank
    text = text + """
int
main() {
  %s
return 0;
}
""" % (call or "")

    if call:
        i = string.find(call, "\n")
        if i > 0:
            calltext = call[:i] + ".."
        elif call[-1] == ';':
            calltext = call[:-1]
        else:
            calltext = call

    for lib_name in libs:

        lang, suffix, msg = _lang2suffix(language)
        if msg:
            context.Display("Cannot check for library %s: %s\n" % (lib_name, msg))
            return msg

        # if a function was specified to run in main(), say it
        if call:
                context.Display("Checking for %s in %s library %s... "
                                % (calltext, lang, lib_name))
        # otherwise, just say the name of library and language
        else:
                context.Display("Checking for %s library %s... "
                                % (lang, lib_name))

        if lib_name:
            l = [ lib_name ]
            if extra_libs:
                l.extend(extra_libs)
            oldLIBS = context.AppendLIBS(l)
            sym = "HAVE_LIB" + lib_name
        else:
            oldLIBS = -1
            sym = None

        ret = context.BuildProg(text, suffix)

        _YesNoResult(context, ret, sym, text,
                     "Define to 1 if you have the `%s' library." % lib_name)
        if oldLIBS != -1 and (ret or not autoadd):
            context.SetLIBS(oldLIBS)
            
        if not ret:
            return ret

    return ret

#
# END OF PUBLIC FUNCTIONS
#

def _YesNoResult(context, ret, key, text, comment = None):
    """
    Handle the result of a test with a "yes" or "no" result.
    "ret" is the return value: empty if OK, error message when not.
    "key" is the name of the symbol to be defined (HAVE_foo).
    "text" is the source code of the program used for testing.
    "comment" is the C comment to add above the line defining the symbol (the
    comment is automatically put inside a /* */). If None, no comment is added.
    """
    if key:
        _Have(context, key, not ret, comment)
    if ret:
        context.Display("no\n")
        _LogFailed(context, text, ret)
    else:
        context.Display("yes\n")


def _Have(context, key, have, comment = None):
    """
    Store result of a test in context.havedict and context.headerfilename.
    "key" is a "HAVE_abc" name.  It is turned into all CAPITALS and non-
    alphanumerics are replaced by an underscore.
    The value of "have" can be:
    1      - Feature is defined, add "#define key".
    0      - Feature is not defined, add "/* #undef key */".
             Adding "undef" is what autoconf does.  Not useful for the
             compiler, but it shows that the test was done.
    number - Feature is defined to this number "#define key have".
             Doesn't work for 0 or 1, use a string then.
    string - Feature is defined to this string "#define key have".
             Give "have" as is should appear in the header file, include quotes
             when desired and escape special characters!
    """
    key_up = string.upper(key)
    key_up = re.sub('[^A-Z0-9_]', '_', key_up)
    context.havedict[key_up] = have
    if have == 1:
        line = "#define %s 1\n" % key_up
    elif have == 0:
        line = "/* #undef %s */\n" % key_up
    elif type(have) == IntType:
        line = "#define %s %d\n" % (key_up, have)
    else:
        line = "#define %s %s\n" % (key_up, str(have))
    
    if comment is not None:
        lines = "\n/* %s */\n" % comment + line
    else:
        lines = "\n" + line

    if context.headerfilename:
        f = open(context.headerfilename, "a")
        f.write(lines)
        f.close()
    elif hasattr(context,'config_h'):
        context.config_h = context.config_h + lines


def _LogFailed(context, text, msg):
    """
    Write to the log about a failed program.
    Add line numbers, so that error messages can be understood.
    """
    if LogInputFiles:
        context.Log("Failed program was:\n")
        lines = string.split(text, '\n')
        if len(lines) and lines[-1] == '':
            lines = lines[:-1]              # remove trailing empty line
        n = 1
        for line in lines:
            context.Log("%d: %s\n" % (n, line))
            n = n + 1
    if LogErrorMessages:
        context.Log("Error message: %s\n" % msg)


def _lang2suffix(lang):
    """
    Convert a language name to a suffix.
    When "lang" is empty or None C is assumed.
    Returns a tuple (lang, suffix, None) when it works.
    For an unrecognized language returns (None, None, msg).
    Where:
        lang   = the unified language name
        suffix = the suffix, including the leading dot
        msg    = an error message
    """
    if not lang or lang in ["C", "c"]:
        return ("C", ".c", None)
    if lang in ["c++", "C++", "cpp", "CXX", "cxx"]:
        return ("C++", ".cpp", None)

    return None, None, "Unsupported language: %s" % lang


# vim: set sw=4 et sts=4 tw=79 fo+=l:
