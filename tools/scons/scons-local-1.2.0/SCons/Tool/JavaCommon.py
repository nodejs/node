"""SCons.Tool.JavaCommon

Stuff for processing Java.

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

__revision__ = "src/engine/SCons/Tool/JavaCommon.py 3842 2008/12/20 22:59:52 scons"

import os
import os.path
import re
import string

java_parsing = 1

default_java_version = '1.4'

if java_parsing:
    # Parse Java files for class names.
    #
    # This is a really cool parser from Charles Crain
    # that finds appropriate class names in Java source.

    # A regular expression that will find, in a java file:
    #     newlines;
    #     double-backslashes;
    #     a single-line comment "//";
    #     single or double quotes preceeded by a backslash;
    #     single quotes, double quotes, open or close braces, semi-colons,
    #         periods, open or close parentheses;
    #     floating-point numbers;
    #     any alphanumeric token (keyword, class name, specifier);
    #     any alphanumeric token surrounded by angle brackets (generics);
    #     the multi-line comment begin and end tokens /* and */;
    #     array declarations "[]".
    _reToken = re.compile(r'(\n|\\\\|//|\\[\'"]|[\'"\{\}\;\.\(\)]|' +
                          r'\d*\.\d*|[A-Za-z_][\w\$\.]*|<[A-Za-z_]\w+>|' +
                          r'/\*|\*/|\[\])')

    class OuterState:
        """The initial state for parsing a Java file for classes,
        interfaces, and anonymous inner classes."""
        def __init__(self, version=default_java_version):

            if not version in ('1.1', '1.2', '1.3','1.4', '1.5', '1.6'):
                msg = "Java version %s not supported" % version
                raise NotImplementedError, msg

            self.version = version
            self.listClasses = []
            self.listOutputs = []
            self.stackBrackets = []
            self.brackets = 0
            self.nextAnon = 1
            self.localClasses = []
            self.stackAnonClassBrackets = []
            self.anonStacksStack = [[0]]
            self.package = None

        def trace(self):
            pass

        def __getClassState(self):
            try:
                return self.classState
            except AttributeError:
                ret = ClassState(self)
                self.classState = ret
                return ret

        def __getPackageState(self):
            try:
                return self.packageState
            except AttributeError:
                ret = PackageState(self)
                self.packageState = ret
                return ret

        def __getAnonClassState(self):
            try:
                return self.anonState
            except AttributeError:
                self.outer_state = self
                ret = SkipState(1, AnonClassState(self))
                self.anonState = ret
                return ret

        def __getSkipState(self):
            try:
                return self.skipState
            except AttributeError:
                ret = SkipState(1, self)
                self.skipState = ret
                return ret
        
        def __getAnonStack(self):
            return self.anonStacksStack[-1]

        def openBracket(self):
            self.brackets = self.brackets + 1

        def closeBracket(self):
            self.brackets = self.brackets - 1
            if len(self.stackBrackets) and \
               self.brackets == self.stackBrackets[-1]:
                self.listOutputs.append(string.join(self.listClasses, '$'))
                self.localClasses.pop()
                self.listClasses.pop()
                self.anonStacksStack.pop()
                self.stackBrackets.pop()
            if len(self.stackAnonClassBrackets) and \
               self.brackets == self.stackAnonClassBrackets[-1]:
                self.__getAnonStack().pop()
                self.stackAnonClassBrackets.pop()

        def parseToken(self, token):
            if token[:2] == '//':
                return IgnoreState('\n', self)
            elif token == '/*':
                return IgnoreState('*/', self)
            elif token == '{':
                self.openBracket()
            elif token == '}':
                self.closeBracket()
            elif token in [ '"', "'" ]:
                return IgnoreState(token, self)
            elif token == "new":
                # anonymous inner class
                if len(self.listClasses) > 0:
                    return self.__getAnonClassState()
                return self.__getSkipState() # Skip the class name
            elif token in ['class', 'interface', 'enum']:
                if len(self.listClasses) == 0:
                    self.nextAnon = 1
                self.stackBrackets.append(self.brackets)
                return self.__getClassState()
            elif token == 'package':
                return self.__getPackageState()
            elif token == '.':
                # Skip the attribute, it might be named "class", in which
                # case we don't want to treat the following token as
                # an inner class name...
                return self.__getSkipState()
            return self

        def addAnonClass(self):
            """Add an anonymous inner class"""
            if self.version in ('1.1', '1.2', '1.3', '1.4'):
                clazz = self.listClasses[0]
                self.listOutputs.append('%s$%d' % (clazz, self.nextAnon))
            elif self.version in ('1.5', '1.6'):
                self.stackAnonClassBrackets.append(self.brackets)
                className = []
                className.extend(self.listClasses)
                self.__getAnonStack()[-1] = self.__getAnonStack()[-1] + 1
                for anon in self.__getAnonStack():
                    className.append(str(anon))
                self.listOutputs.append(string.join(className, '$'))

            self.nextAnon = self.nextAnon + 1
            self.__getAnonStack().append(0)

        def setPackage(self, package):
            self.package = package

    class AnonClassState:
        """A state that looks for anonymous inner classes."""
        def __init__(self, old_state):
            # outer_state is always an instance of OuterState
            self.outer_state = old_state.outer_state
            self.old_state = old_state
            self.brace_level = 0
        def parseToken(self, token):
            # This is an anonymous class if and only if the next
            # non-whitespace token is a bracket. Everything between
            # braces should be parsed as normal java code.
            if token[:2] == '//':
                return IgnoreState('\n', self)
            elif token == '/*':
                return IgnoreState('*/', self)
            elif token == '\n':
                return self
            elif token[0] == '<' and token[-1] == '>':
                return self
            elif token == '(':
                self.brace_level = self.brace_level + 1
                return self
            if self.brace_level > 0:
                if token == 'new':
                    # look further for anonymous inner class
                    return SkipState(1, AnonClassState(self))
                elif token in [ '"', "'" ]:
                    return IgnoreState(token, self)
                elif token == ')':
                    self.brace_level = self.brace_level - 1
                return self
            if token == '{':
                self.outer_state.addAnonClass()
            return self.old_state.parseToken(token)

    class SkipState:
        """A state that will skip a specified number of tokens before
        reverting to the previous state."""
        def __init__(self, tokens_to_skip, old_state):
            self.tokens_to_skip = tokens_to_skip
            self.old_state = old_state
        def parseToken(self, token):
            self.tokens_to_skip = self.tokens_to_skip - 1
            if self.tokens_to_skip < 1:
                return self.old_state
            return self

    class ClassState:
        """A state we go into when we hit a class or interface keyword."""
        def __init__(self, outer_state):
            # outer_state is always an instance of OuterState
            self.outer_state = outer_state
        def parseToken(self, token):
            # the next non-whitespace token should be the name of the class
            if token == '\n':
                return self
            # If that's an inner class which is declared in a method, it
            # requires an index prepended to the class-name, e.g.
            # 'Foo$1Inner' (Tigris Issue 2087)
            if self.outer_state.localClasses and \
                self.outer_state.stackBrackets[-1] > \
                self.outer_state.stackBrackets[-2]+1:
                locals = self.outer_state.localClasses[-1]
                try:
                    idx = locals[token]
                    locals[token] = locals[token]+1
                except KeyError:
                    locals[token] = 1
                token = str(locals[token]) + token
            self.outer_state.localClasses.append({})
            self.outer_state.listClasses.append(token)
            self.outer_state.anonStacksStack.append([0])
            return self.outer_state

    class IgnoreState:
        """A state that will ignore all tokens until it gets to a
        specified token."""
        def __init__(self, ignore_until, old_state):
            self.ignore_until = ignore_until
            self.old_state = old_state
        def parseToken(self, token):
            if self.ignore_until == token:
                return self.old_state
            return self

    class PackageState:
        """The state we enter when we encounter the package keyword.
        We assume the next token will be the package name."""
        def __init__(self, outer_state):
            # outer_state is always an instance of OuterState
            self.outer_state = outer_state
        def parseToken(self, token):
            self.outer_state.setPackage(token)
            return self.outer_state

    def parse_java_file(fn, version=default_java_version):
        return parse_java(open(fn, 'r').read(), version)

    def parse_java(contents, version=default_java_version, trace=None):
        """Parse a .java file and return a double of package directory,
        plus a list of .class files that compiling that .java file will
        produce"""
        package = None
        initial = OuterState(version)
        currstate = initial
        for token in _reToken.findall(contents):
            # The regex produces a bunch of groups, but only one will
            # have anything in it.
            currstate = currstate.parseToken(token)
            if trace: trace(token, currstate)
        if initial.package:
            package = string.replace(initial.package, '.', os.sep)
        return (package, initial.listOutputs)

else:
    # Don't actually parse Java files for class names.
    #
    # We might make this a configurable option in the future if
    # Java-file parsing takes too long (although it shouldn't relative
    # to how long the Java compiler itself seems to take...).

    def parse_java_file(fn):
        """ "Parse" a .java file.

        This actually just splits the file name, so the assumption here
        is that the file name matches the public class name, and that
        the path to the file is the same as the package name.
        """
        return os.path.split(file)
