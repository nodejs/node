
"""scons.Node.Alias

Alias nodes.

This creates a hash of global Aliases (dummy targets).

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

__revision__ = "src/engine/SCons/Node/Alias.py 3842 2008/12/20 22:59:52 scons"

import string
import UserDict

import SCons.Errors
import SCons.Node
import SCons.Util

class AliasNameSpace(UserDict.UserDict):
    def Alias(self, name, **kw):
        if isinstance(name, SCons.Node.Alias.Alias):
            return name
        try:
            a = self[name]
        except KeyError:
            a = apply(SCons.Node.Alias.Alias, (name,), kw)
            self[name] = a
        return a

    def lookup(self, name, **kw):
        try:
            return self[name]
        except KeyError:
            return None

class AliasNodeInfo(SCons.Node.NodeInfoBase):
    current_version_id = 1
    field_list = ['csig']
    def str_to_node(self, s):
        return default_ans.Alias(s)

class AliasBuildInfo(SCons.Node.BuildInfoBase):
    current_version_id = 1

class Alias(SCons.Node.Node):

    NodeInfo = AliasNodeInfo
    BuildInfo = AliasBuildInfo

    def __init__(self, name):
        SCons.Node.Node.__init__(self)
        self.name = name

    def str_for_display(self):
        return '"' + self.__str__() + '"'

    def __str__(self):
        return self.name

    def make_ready(self):
        self.get_csig()

    really_build = SCons.Node.Node.build
    is_up_to_date = SCons.Node.Node.children_are_up_to_date

    def is_under(self, dir):
        # Make Alias nodes get built regardless of
        # what directory scons was run from. Alias nodes
        # are outside the filesystem:
        return 1

    def get_contents(self):
        """The contents of an alias is the concatenation
        of the content signatures of all its sources."""
        childsigs = map(lambda n: n.get_csig(), self.children())
        return string.join(childsigs, '')

    def sconsign(self):
        """An Alias is not recorded in .sconsign files"""
        pass

    #
    #
    #

    def changed_since_last_build(self, target, prev_ni):
        cur_csig = self.get_csig()
        try:
            return cur_csig != prev_ni.csig
        except AttributeError:
            return 1

    def build(self):
        """A "builder" for aliases."""
        pass

    def convert(self):
        try: del self.builder
        except AttributeError: pass
        self.reset_executor()
        self.build = self.really_build

    def get_csig(self):
        """
        Generate a node's content signature, the digested signature
        of its content.

        node - the node
        cache - alternate node to use for the signature cache
        returns - the content signature
        """
        try:
            return self.ninfo.csig
        except AttributeError:
            pass

        contents = self.get_contents()
        csig = SCons.Util.MD5signature(contents)
        self.get_ninfo().csig = csig
        return csig

default_ans = AliasNameSpace()

SCons.Node.arg2nodes_lookups.append(default_ans.lookup)
