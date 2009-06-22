"""SCons.Scanner

The Scanner package for the SCons software construction utility.

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

__revision__ = "src/engine/SCons/Scanner/__init__.py 3842 2008/12/20 22:59:52 scons"

import re
import string

import SCons.Node.FS
import SCons.Util


class _Null:
    pass

# This is used instead of None as a default argument value so None can be
# used as an actual argument value.
_null = _Null

def Scanner(function, *args, **kw):
    """
    Public interface factory function for creating different types
    of Scanners based on the different types of "functions" that may
    be supplied.

    TODO:  Deprecate this some day.  We've moved the functionality
    inside the Base class and really don't need this factory function
    any more.  It was, however, used by some of our Tool modules, so
    the call probably ended up in various people's custom modules
    patterned on SCons code.
    """
    if SCons.Util.is_Dict(function):
        return apply(Selector, (function,) + args, kw)
    else:
        return apply(Base, (function,) + args, kw)



class FindPathDirs:
    """A class to bind a specific *PATH variable name to a function that
    will return all of the *path directories."""
    def __init__(self, variable):
        self.variable = variable
    def __call__(self, env, dir=None, target=None, source=None, argument=None):
        import SCons.PathList
        try:
            path = env[self.variable]
        except KeyError:
            return ()

        dir = dir or env.fs._cwd
        path = SCons.PathList.PathList(path).subst_path(env, target, source)
        return tuple(dir.Rfindalldirs(path))



class Base:
    """
    The base class for dependency scanners.  This implements
    straightforward, single-pass scanning of a single file.
    """

    def __init__(self,
                 function,
                 name = "NONE",
                 argument = _null,
                 skeys = _null,
                 path_function = None,
                 node_class = SCons.Node.FS.Entry,
                 node_factory = None,
                 scan_check = None,
                 recursive = None):
        """
        Construct a new scanner object given a scanner function.

        'function' - a scanner function taking two or three
        arguments and returning a list of strings.

        'name' - a name for identifying this scanner object.

        'argument' - an optional argument that, if specified, will be
        passed to both the scanner function and the path_function.

        'skeys' - an optional list argument that can be used to determine
        which scanner should be used for a given Node. In the case of File
        nodes, for example, the 'skeys' would be file suffixes.

        'path_function' - a function that takes four or five arguments
        (a construction environment, Node for the directory containing
        the SConscript file that defined the primary target, list of
        target nodes, list of source nodes, and optional argument for
        this instance) and returns a tuple of the directories that can
        be searched for implicit dependency files.  May also return a
        callable() which is called with no args and returns the tuple
        (supporting Bindable class).

        'node_class' - the class of Nodes which this scan will return.
        If node_class is None, then this scanner will not enforce any
        Node conversion and will return the raw results from the
        underlying scanner function.

        'node_factory' - the factory function to be called to translate
        the raw results returned by the scanner function into the
        expected node_class objects.

        'scan_check' - a function to be called to first check whether
        this node really needs to be scanned.

        'recursive' - specifies that this scanner should be invoked
        recursively on all of the implicit dependencies it returns
        (the canonical example being #include lines in C source files).
        May be a callable, which will be called to filter the list
        of nodes found to select a subset for recursive scanning
        (the canonical example being only recursively scanning
        subdirectories within a directory).

        The scanner function's first argument will be a Node that should
        be scanned for dependencies, the second argument will be an
        Environment object, the third argument will be the tuple of paths
        returned by the path_function, and the fourth argument will be
        the value passed into 'argument', and the returned list should
        contain the Nodes for all the direct dependencies of the file.

        Examples:

        s = Scanner(my_scanner_function)

        s = Scanner(function = my_scanner_function)

        s = Scanner(function = my_scanner_function, argument = 'foo')

        """

        # Note: this class could easily work with scanner functions that take
        # something other than a filename as an argument (e.g. a database
        # node) and a dependencies list that aren't file names. All that
        # would need to be changed is the documentation.

        self.function = function
        self.path_function = path_function
        self.name = name
        self.argument = argument

        if skeys is _null:
            if SCons.Util.is_Dict(function):
                skeys = function.keys()
            else:
                skeys = []
        self.skeys = skeys

        self.node_class = node_class
        self.node_factory = node_factory
        self.scan_check = scan_check
        if callable(recursive):
            self.recurse_nodes = recursive
        elif recursive:
            self.recurse_nodes = self._recurse_all_nodes
        else:
            self.recurse_nodes = self._recurse_no_nodes

    def path(self, env, dir=None, target=None, source=None):
        if not self.path_function:
            return ()
        if not self.argument is _null:
            return self.path_function(env, dir, target, source, self.argument)
        else:
            return self.path_function(env, dir, target, source)

    def __call__(self, node, env, path = ()):
        """
        This method scans a single object. 'node' is the node
        that will be passed to the scanner function, and 'env' is the
        environment that will be passed to the scanner function. A list of
        direct dependency nodes for the specified node will be returned.
        """
        if self.scan_check and not self.scan_check(node, env):
            return []

        self = self.select(node)

        if not self.argument is _null:
            list = self.function(node, env, path, self.argument)
        else:
            list = self.function(node, env, path)

        kw = {}
        if hasattr(node, 'dir'):
            kw['directory'] = node.dir
        node_factory = env.get_factory(self.node_factory)
        nodes = []
        for l in list:
            if self.node_class and not isinstance(l, self.node_class):
                l = apply(node_factory, (l,), kw)
            nodes.append(l)
        return nodes

    def __cmp__(self, other):
        try:
            return cmp(self.__dict__, other.__dict__)
        except AttributeError:
            # other probably doesn't have a __dict__
            return cmp(self.__dict__, other)

    def __hash__(self):
        return id(self)

    def __str__(self):
        return self.name

    def add_skey(self, skey):
        """Add a skey to the list of skeys"""
        self.skeys.append(skey)

    def get_skeys(self, env=None):
        if env and SCons.Util.is_String(self.skeys):
            return env.subst_list(self.skeys)[0]
        return self.skeys

    def select(self, node):
        if SCons.Util.is_Dict(self.function):
            key = node.scanner_key()
            try:
                return self.function[key]
            except KeyError:
                return None
        else:
            return self

    def _recurse_all_nodes(self, nodes):
        return nodes

    def _recurse_no_nodes(self, nodes):
        return []

    recurse_nodes = _recurse_no_nodes

    def add_scanner(self, skey, scanner):
        self.function[skey] = scanner
        self.add_skey(skey)


class Selector(Base):
    """
    A class for selecting a more specific scanner based on the
    scanner_key() (suffix) for a specific Node.

    TODO:  This functionality has been moved into the inner workings of
    the Base class, and this class will be deprecated at some point.
    (It was never exposed directly as part of the public interface,
    although it is used by the Scanner() factory function that was
    used by various Tool modules and therefore was likely a template
    for custom modules that may be out there.)
    """
    def __init__(self, dict, *args, **kw):
        apply(Base.__init__, (self, None,)+args, kw)
        self.dict = dict
        self.skeys = dict.keys()

    def __call__(self, node, env, path = ()):
        return self.select(node)(node, env, path)

    def select(self, node):
        try:
            return self.dict[node.scanner_key()]
        except KeyError:
            return None

    def add_scanner(self, skey, scanner):
        self.dict[skey] = scanner
        self.add_skey(skey)


class Current(Base):
    """
    A class for scanning files that are source files (have no builder)
    or are derived files and are current (which implies that they exist,
    either locally or in a repository).
    """

    def __init__(self, *args, **kw):
        def current_check(node, env):
            return not node.has_builder() or node.is_up_to_date()
        kw['scan_check'] = current_check
        apply(Base.__init__, (self,) + args, kw)

class Classic(Current):
    """
    A Scanner subclass to contain the common logic for classic CPP-style
    include scanning, but which can be customized to use different
    regular expressions to find the includes.

    Note that in order for this to work "out of the box" (without
    overriding the find_include() and sort_key() methods), the regular
    expression passed to the constructor must return the name of the
    include file in group 0.
    """

    def __init__(self, name, suffixes, path_variable, regex, *args, **kw):

        self.cre = re.compile(regex, re.M)

        def _scan(node, env, path=(), self=self):
            node = node.rfile()
            if not node.exists():
                return []
            return self.scan(node, path)

        kw['function'] = _scan
        kw['path_function'] = FindPathDirs(path_variable)
        kw['recursive'] = 1
        kw['skeys'] = suffixes
        kw['name'] = name

        apply(Current.__init__, (self,) + args, kw)

    def find_include(self, include, source_dir, path):
        n = SCons.Node.FS.find_file(include, (source_dir,) + tuple(path))
        return n, include

    def sort_key(self, include):
        return SCons.Node.FS._my_normcase(include)

    def find_include_names(self, node):
        return self.cre.findall(node.get_contents())

    def scan(self, node, path=()):

        # cache the includes list in node so we only scan it once:
        if node.includes != None:
            includes = node.includes
        else:
            includes = self.find_include_names (node)
            node.includes = includes

        # This is a hand-coded DSU (decorate-sort-undecorate, or
        # Schwartzian transform) pattern.  The sort key is the raw name
        # of the file as specifed on the #include line (including the
        # " or <, since that may affect what file is found), which lets
        # us keep the sort order constant regardless of whether the file
        # is actually found in a Repository or locally.
        nodes = []
        source_dir = node.get_dir()
        if callable(path):
            path = path()
        for include in includes:
            n, i = self.find_include(include, source_dir, path)

            if n is None:
                SCons.Warnings.warn(SCons.Warnings.DependencyWarning,
                                    "No dependency generated for file: %s (included from: %s) -- file not found" % (i, node))
            else:
                sortkey = self.sort_key(include)
                nodes.append((sortkey, n))

        nodes.sort()
        nodes = map(lambda pair: pair[1], nodes)
        return nodes

class ClassicCPP(Classic):
    """
    A Classic Scanner subclass which takes into account the type of
    bracketing used to include the file, and uses classic CPP rules
    for searching for the files based on the bracketing.

    Note that in order for this to work, the regular expression passed
    to the constructor must return the leading bracket in group 0, and
    the contained filename in group 1.
    """
    def find_include(self, include, source_dir, path):
        if include[0] == '"':
            paths = (source_dir,) + tuple(path)
        else:
            paths = tuple(path) + (source_dir,)

        n = SCons.Node.FS.find_file(include[1], paths)

        return n, include[1]

    def sort_key(self, include):
        return SCons.Node.FS._my_normcase(string.join(include))
