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

__revision__ = "src/engine/SCons/cpp.py 3842 2008/12/20 22:59:52 scons"

__doc__ = """
SCons C Pre-Processor module
"""

# TODO(1.5):  remove this import
# This module doesn't use anything from SCons by name, but we import SCons
# here to pull in zip() from the SCons.compat layer for early Pythons.
import SCons

import os
import re
import string

#
# First "subsystem" of regular expressions that we set up:
#
# Stuff to turn the C preprocessor directives in a file's contents into
# a list of tuples that we can process easily.
#

# A table of regular expressions that fetch the arguments from the rest of
# a C preprocessor line.  Different directives have different arguments
# that we want to fetch, using the regular expressions to which the lists
# of preprocessor directives map.
cpp_lines_dict = {
    # Fetch the rest of a #if/#elif/#ifdef/#ifndef as one argument,
    # separated from the keyword by white space.
    ('if', 'elif', 'ifdef', 'ifndef',)
                        : '\s+(.+)',

    # Fetch the rest of a #import/#include/#include_next line as one
    # argument, with white space optional.
    ('import', 'include', 'include_next',)
                        : '\s*(.+)',

    # We don't care what comes after a #else or #endif line.
    ('else', 'endif',)  : '',

    # Fetch three arguments from a #define line:
    #   1) The #defined keyword.
    #   2) The optional parentheses and arguments (if it's a function-like
    #      macro, '' if it's not).
    #   3) The expansion value.
    ('define',)         : '\s+([_A-Za-z][_A-Za-z0-9_]+)(\([^)]*\))?\s*(.*)',

    # Fetch the #undefed keyword from a #undef line.
    ('undef',)          : '\s+([_A-Za-z][A-Za-z0-9_]+)',
}

# Create a table that maps each individual C preprocessor directive to
# the corresponding compiled regular expression that fetches the arguments
# we care about.
Table = {}
for op_list, expr in cpp_lines_dict.items():
    e = re.compile(expr)
    for op in op_list:
        Table[op] = e
del e
del op
del op_list

# Create a list of the expressions we'll use to match all of the
# preprocessor directives.  These are the same as the directives
# themselves *except* that we must use a negative lookahead assertion
# when matching "if" so it doesn't match the "if" in "ifdef."
override = {
    'if'                        : 'if(?!def)',
}
l = map(lambda x, o=override: o.get(x, x), Table.keys())


# Turn the list of expressions into one big honkin' regular expression
# that will match all the preprocessor lines at once.  This will return
# a list of tuples, one for each preprocessor line.  The preprocessor
# directive will be the first element in each tuple, and the rest of
# the line will be the second element.
e = '^\s*#\s*(' + string.join(l, '|') + ')(.*)$'

# And last but not least, compile the expression.
CPP_Expression = re.compile(e, re.M)




#
# Second "subsystem" of regular expressions that we set up:
#
# Stuff to translate a C preprocessor expression (as found on a #if or
# #elif line) into an equivalent Python expression that we can eval().
#

# A dictionary that maps the C representation of Boolean operators
# to their Python equivalents.
CPP_to_Python_Ops_Dict = {
    '!'         : ' not ',
    '!='        : ' != ',
    '&&'        : ' and ',
    '||'        : ' or ',
    '?'         : ' and ',
    ':'         : ' or ',
    '\r'        : '',
}

CPP_to_Python_Ops_Sub = lambda m, d=CPP_to_Python_Ops_Dict: d[m.group(0)]

# We have to sort the keys by length so that longer expressions
# come *before* shorter expressions--in particular, "!=" must
# come before "!" in the alternation.  Without this, the Python
# re module, as late as version 2.2.2, empirically matches the
# "!" in "!=" first, instead of finding the longest match.
# What's up with that?
l = CPP_to_Python_Ops_Dict.keys()
l.sort(lambda a, b: cmp(len(b), len(a)))

# Turn the list of keys into one regular expression that will allow us
# to substitute all of the operators at once.
expr = string.join(map(re.escape, l), '|')

# ...and compile the expression.
CPP_to_Python_Ops_Expression = re.compile(expr)

# A separate list of expressions to be evaluated and substituted
# sequentially, not all at once.
CPP_to_Python_Eval_List = [
    ['defined\s+(\w+)',         '__dict__.has_key("\\1")'],
    ['defined\s*\((\w+)\)',     '__dict__.has_key("\\1")'],
    ['/\*.*\*/',                ''],
    ['/\*.*',                   ''],
    ['//.*',                    ''],
    ['(0x[0-9A-Fa-f]*)[UL]+',   '\\1L'],
]

# Replace the string representations of the regular expressions in the
# list with compiled versions.
for l in CPP_to_Python_Eval_List:
    l[0] = re.compile(l[0])

# Wrap up all of the above into a handy function.
def CPP_to_Python(s):
    """
    Converts a C pre-processor expression into an equivalent
    Python expression that can be evaluated.
    """
    s = CPP_to_Python_Ops_Expression.sub(CPP_to_Python_Ops_Sub, s)
    for expr, repl in CPP_to_Python_Eval_List:
        s = expr.sub(repl, s)
    return s



del expr
del l
del override



class FunctionEvaluator:
    """
    Handles delayed evaluation of a #define function call.
    """
    def __init__(self, name, args, expansion):
        """
        Squirrels away the arguments and expansion value of a #define
        macro function for later evaluation when we must actually expand
        a value that uses it.
        """
        self.name = name
        self.args = function_arg_separator.split(args)
        try:
            expansion = string.split(expansion, '##')
        except (AttributeError, TypeError):
            # Python 1.5 throws TypeError if "expansion" isn't a string,
            # later versions throw AttributeError.
            pass
        self.expansion = expansion
    def __call__(self, *values):
        """
        Evaluates the expansion of a #define macro function called
        with the specified values.
        """
        if len(self.args) != len(values):
            raise ValueError, "Incorrect number of arguments to `%s'" % self.name
        # Create a dictionary that maps the macro arguments to the
        # corresponding values in this "call."  We'll use this when we
        # eval() the expansion so that arguments will get expanded to
        # the right values.
        locals = {}
        for k, v in zip(self.args, values):
            locals[k] = v

        parts = []
        for s in self.expansion:
            if not s in self.args:
                s = repr(s)
            parts.append(s)
        statement = string.join(parts, ' + ')

        return eval(statement, globals(), locals)



# Find line continuations.
line_continuations = re.compile('\\\\\r?\n')

# Search for a "function call" macro on an expansion.  Returns the
# two-tuple of the "function" name itself, and a string containing the
# arguments within the call parentheses.
function_name = re.compile('(\S+)\(([^)]*)\)')

# Split a string containing comma-separated function call arguments into
# the separate arguments.
function_arg_separator = re.compile(',\s*')



class PreProcessor:
    """
    The main workhorse class for handling C pre-processing.
    """
    def __init__(self, current=os.curdir, cpppath=(), dict={}, all=0):
        global Table

        cpppath = tuple(cpppath)

        self.searchpath = {
            '"' :       (current,) + cpppath,
            '<' :       cpppath + (current,),
        }

        # Initialize our C preprocessor namespace for tracking the
        # values of #defined keywords.  We use this namespace to look
        # for keywords on #ifdef/#ifndef lines, and to eval() the
        # expressions on #if/#elif lines (after massaging them from C to
        # Python).
        self.cpp_namespace = dict.copy()
        self.cpp_namespace['__dict__'] = self.cpp_namespace

        if all:
           self.do_include = self.all_include

        # For efficiency, a dispatch table maps each C preprocessor
        # directive (#if, #define, etc.) to the method that should be
        # called when we see it.  We accomodate state changes (#if,
        # #ifdef, #ifndef) by pushing the current dispatch table on a
        # stack and changing what method gets called for each relevant
        # directive we might see next at this level (#else, #elif).
        # #endif will simply pop the stack.
        d = {
            'scons_current_file'    : self.scons_current_file
        }
        for op in Table.keys():
            d[op] = getattr(self, 'do_' + op)
        self.default_table = d

    # Controlling methods.

    def tupleize(self, contents):
        """
        Turns the contents of a file into a list of easily-processed
        tuples describing the CPP lines in the file.

        The first element of each tuple is the line's preprocessor
        directive (#if, #include, #define, etc., minus the initial '#').
        The remaining elements are specific to the type of directive, as
        pulled apart by the regular expression.
        """
        global CPP_Expression, Table
        contents = line_continuations.sub('', contents)
        cpp_tuples = CPP_Expression.findall(contents)
        return  map(lambda m, t=Table:
                           (m[0],) + t[m[0]].match(m[1]).groups(),
                    cpp_tuples)

    def __call__(self, file):
        """
        Pre-processes a file.

        This is the main public entry point.
        """
        self.current_file = file
        return self.process_contents(self.read_file(file), file)

    def process_contents(self, contents, fname=None):
        """
        Pre-processes a file contents.

        This is the main internal entry point.
        """
        self.stack = []
        self.dispatch_table = self.default_table.copy()
        self.current_file = fname
        self.tuples = self.tupleize(contents)

        self.initialize_result(fname)
        while self.tuples:
            t = self.tuples.pop(0)
            # Uncomment to see the list of tuples being processed (e.g.,
            # to validate the CPP lines are being translated correctly).
            #print t
            self.dispatch_table[t[0]](t)
        return self.finalize_result(fname)

    # Dispatch table stack manipulation methods.

    def save(self):
        """
        Pushes the current dispatch table on the stack and re-initializes
        the current dispatch table to the default.
        """
        self.stack.append(self.dispatch_table)
        self.dispatch_table = self.default_table.copy()

    def restore(self):
        """
        Pops the previous dispatch table off the stack and makes it the
        current one.
        """
        try: self.dispatch_table = self.stack.pop()
        except IndexError: pass

    # Utility methods.

    def do_nothing(self, t):
        """
        Null method for when we explicitly want the action for a
        specific preprocessor directive to do nothing.
        """
        pass

    def scons_current_file(self, t):
        self.current_file = t[1]

    def eval_expression(self, t):
        """
        Evaluates a C preprocessor expression.

        This is done by converting it to a Python equivalent and
        eval()ing it in the C preprocessor namespace we use to
        track #define values.
        """
        t = CPP_to_Python(string.join(t[1:]))
        try: return eval(t, self.cpp_namespace)
        except (NameError, TypeError): return 0

    def initialize_result(self, fname):
        self.result = [fname]

    def finalize_result(self, fname):
        return self.result[1:]

    def find_include_file(self, t):
        """
        Finds the #include file for a given preprocessor tuple.
        """
        fname = t[2]
        for d in self.searchpath[t[1]]:
            if d == os.curdir:
                f = fname
            else:
                f = os.path.join(d, fname)
            if os.path.isfile(f):
                return f
        return None

    def read_file(self, file):
        return open(file).read()

    # Start and stop processing include lines.

    def start_handling_includes(self, t=None):
        """
        Causes the PreProcessor object to start processing #import,
        #include and #include_next lines.

        This method will be called when a #if, #ifdef, #ifndef or #elif
        evaluates True, or when we reach the #else in a #if, #ifdef,
        #ifndef or #elif block where a condition already evaluated
        False.

        """
        d = self.dispatch_table
        d['import'] = self.do_import
        d['include'] =  self.do_include
        d['include_next'] =  self.do_include

    def stop_handling_includes(self, t=None):
        """
        Causes the PreProcessor object to stop processing #import,
        #include and #include_next lines.

        This method will be called when a #if, #ifdef, #ifndef or #elif
        evaluates False, or when we reach the #else in a #if, #ifdef,
        #ifndef or #elif block where a condition already evaluated True.
        """
        d = self.dispatch_table
        d['import'] = self.do_nothing
        d['include'] =  self.do_nothing
        d['include_next'] =  self.do_nothing

    # Default methods for handling all of the preprocessor directives.
    # (Note that what actually gets called for a given directive at any
    # point in time is really controlled by the dispatch_table.)

    def _do_if_else_condition(self, condition):
        """
        Common logic for evaluating the conditions on #if, #ifdef and
        #ifndef lines.
        """
        self.save()
        d = self.dispatch_table
        if condition:
            self.start_handling_includes()
            d['elif'] = self.stop_handling_includes
            d['else'] = self.stop_handling_includes
        else:
            self.stop_handling_includes()
            d['elif'] = self.do_elif
            d['else'] = self.start_handling_includes

    def do_ifdef(self, t):
        """
        Default handling of a #ifdef line.
        """
        self._do_if_else_condition(self.cpp_namespace.has_key(t[1]))

    def do_ifndef(self, t):
        """
        Default handling of a #ifndef line.
        """
        self._do_if_else_condition(not self.cpp_namespace.has_key(t[1]))

    def do_if(self, t):
        """
        Default handling of a #if line.
        """
        self._do_if_else_condition(self.eval_expression(t))

    def do_elif(self, t):
        """
        Default handling of a #elif line.
        """
        d = self.dispatch_table
        if self.eval_expression(t):
            self.start_handling_includes()
            d['elif'] = self.stop_handling_includes
            d['else'] = self.stop_handling_includes

    def do_else(self, t):
        """
        Default handling of a #else line.
        """
        pass

    def do_endif(self, t):
        """
        Default handling of a #endif line.
        """
        self.restore()

    def do_define(self, t):
        """
        Default handling of a #define line.
        """
        _, name, args, expansion = t
        try:
            expansion = int(expansion)
        except (TypeError, ValueError):
            pass
        if args:
            evaluator = FunctionEvaluator(name, args[1:-1], expansion)
            self.cpp_namespace[name] = evaluator
        else:
            self.cpp_namespace[name] = expansion

    def do_undef(self, t):
        """
        Default handling of a #undef line.
        """
        try: del self.cpp_namespace[t[1]]
        except KeyError: pass

    def do_import(self, t):
        """
        Default handling of a #import line.
        """
        # XXX finish this -- maybe borrow/share logic from do_include()...?
        pass

    def do_include(self, t):
        """
        Default handling of a #include line.
        """
        t = self.resolve_include(t)
        include_file = self.find_include_file(t)
        if include_file:
            #print "include_file =", include_file
            self.result.append(include_file)
            contents = self.read_file(include_file)
            new_tuples = [('scons_current_file', include_file)] + \
                         self.tupleize(contents) + \
                         [('scons_current_file', self.current_file)]
            self.tuples[:] = new_tuples + self.tuples

    # Date: Tue, 22 Nov 2005 20:26:09 -0500
    # From: Stefan Seefeld <seefeld@sympatico.ca>
    #
    # By the way, #include_next is not the same as #include. The difference
    # being that #include_next starts its search in the path following the
    # path that let to the including file. In other words, if your system
    # include paths are ['/foo', '/bar'], and you are looking at a header
    # '/foo/baz.h', it might issue an '#include_next <baz.h>' which would
    # correctly resolve to '/bar/baz.h' (if that exists), but *not* see
    # '/foo/baz.h' again. See http://www.delorie.com/gnu/docs/gcc/cpp_11.html
    # for more reasoning.
    #
    # I have no idea in what context 'import' might be used.

    # XXX is #include_next really the same as #include ?
    do_include_next = do_include

    # Utility methods for handling resolution of include files.

    def resolve_include(self, t):
        """Resolve a tuple-ized #include line.

        This handles recursive expansion of values without "" or <>
        surrounding the name until an initial " or < is found, to handle
                #include FILE
        where FILE is a #define somewhere else.
        """
        s = t[1]
        while not s[0] in '<"':
            #print "s =", s
            try:
                s = self.cpp_namespace[s]
            except KeyError:
                m = function_name.search(s)
                s = self.cpp_namespace[m.group(1)]
                if callable(s):
                    args = function_arg_separator.split(m.group(2))
                    s = apply(s, args)
            if not s:
                return None
        return (t[0], s[0], s[1:-1])

    def all_include(self, t):
        """
        """
        self.result.append(self.resolve_include(t))

class DumbPreProcessor(PreProcessor):
    """A preprocessor that ignores all #if/#elif/#else/#endif directives
    and just reports back *all* of the #include files (like the classic
    SCons scanner did).

    This is functionally equivalent to using a regular expression to
    find all of the #include lines, only slower.  It exists mainly as
    an example of how the main PreProcessor class can be sub-classed
    to tailor its behavior.
    """
    def __init__(self, *args, **kw):
        apply(PreProcessor.__init__, (self,)+args, kw)
        d = self.default_table
        for func in ['if', 'elif', 'else', 'endif', 'ifdef', 'ifndef']:
            d[func] = d[func] = self.do_nothing

del __revision__
