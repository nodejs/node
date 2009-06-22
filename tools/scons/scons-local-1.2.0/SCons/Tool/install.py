"""SCons.Tool.install

Tool-specific initialization for the install tool.

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

__revision__ = "src/engine/SCons/Tool/install.py 3842 2008/12/20 22:59:52 scons"

import os
import shutil
import stat

import SCons.Action
from SCons.Util import make_path_relative

#
# We keep track of *all* installed files.
_INSTALLED_FILES = []
_UNIQUE_INSTALLED_FILES = None

#
# Functions doing the actual work of the Install Builder.
#
def copyFunc(dest, source, env):
    """Install a source file or directory into a destination by copying,
    (including copying permission/mode bits)."""

    if os.path.isdir(source):
        if os.path.exists(dest):
            if not os.path.isdir(dest):
                raise SCons.Errors.UserError, "cannot overwrite non-directory `%s' with a directory `%s'" % (str(dest), str(source))
        else:
            parent = os.path.split(dest)[0]
            if not os.path.exists(parent):
                os.makedirs(parent)
        shutil.copytree(source, dest)
    else:
        shutil.copy2(source, dest)
        st = os.stat(source)
        os.chmod(dest, stat.S_IMODE(st[stat.ST_MODE]) | stat.S_IWRITE)

    return 0

def installFunc(target, source, env):
    """Install a source file into a target using the function specified
    as the INSTALL construction variable."""
    try:
        install = env['INSTALL']
    except KeyError:
        raise SCons.Errors.UserError('Missing INSTALL construction variable.')

    assert len(target)==len(source), \
           "Installing source %s into target %s: target and source lists must have same length."%(map(str, source), map(str, target))
    for t,s in zip(target,source):
        if install(t.get_path(),s.get_path(),env):
            return 1

    return 0

def stringFunc(target, source, env):
    installstr = env.get('INSTALLSTR')
    if installstr:
        return env.subst_target_source(installstr, 0, target, source)
    target = str(target[0])
    source = str(source[0])
    if os.path.isdir(source):
        type = 'directory'
    else:
        type = 'file'
    return 'Install %s: "%s" as "%s"' % (type, source, target)

#
# Emitter functions
#
def add_targets_to_INSTALLED_FILES(target, source, env):
    """ an emitter that adds all target files to the list stored in the
    _INSTALLED_FILES global variable. This way all installed files of one
    scons call will be collected.
    """
    global _INSTALLED_FILES, _UNIQUE_INSTALLED_FILES
    _INSTALLED_FILES.extend(target)
    _UNIQUE_INSTALLED_FILES = None
    return (target, source)

class DESTDIR_factory:
    """ a node factory, where all files will be relative to the dir supplied
    in the constructor.
    """
    def __init__(self, env, dir):
        self.env = env
        self.dir = env.arg2nodes( dir, env.fs.Dir )[0]

    def Entry(self, name):
        name = make_path_relative(name)
        return self.dir.Entry(name)

    def Dir(self, name):
        name = make_path_relative(name)
        return self.dir.Dir(name)

#
# The Builder Definition
#
install_action   = SCons.Action.Action(installFunc, stringFunc)
installas_action = SCons.Action.Action(installFunc, stringFunc)

BaseInstallBuilder               = None

def InstallBuilderWrapper(env, target=None, source=None, dir=None, **kw):
    if target and dir:
        import SCons.Errors
        raise SCons.Errors.UserError, "Both target and dir defined for Install(), only one may be defined."
    if not dir:
        dir=target

    import SCons.Script
    install_sandbox = SCons.Script.GetOption('install_sandbox')
    if install_sandbox:
        target_factory = DESTDIR_factory(env, install_sandbox)
    else:
        target_factory = env.fs

    try:
        dnodes = env.arg2nodes(dir, target_factory.Dir)
    except TypeError:
        raise SCons.Errors.UserError, "Target `%s' of Install() is a file, but should be a directory.  Perhaps you have the Install() arguments backwards?" % str(dir)
    sources = env.arg2nodes(source, env.fs.Entry)
    tgt = []
    for dnode in dnodes:
        for src in sources:
            # Prepend './' so the lookup doesn't interpret an initial
            # '#' on the file name portion as meaning the Node should
            # be relative to the top-level SConstruct directory.
            target = env.fs.Entry('.'+os.sep+src.name, dnode)
            #tgt.extend(BaseInstallBuilder(env, target, src, **kw))
            tgt.extend(apply(BaseInstallBuilder, (env, target, src), kw))
    return tgt

def InstallAsBuilderWrapper(env, target=None, source=None, **kw):
    result = []
    for src, tgt in map(lambda x, y: (x, y), source, target):
        #result.extend(BaseInstallBuilder(env, tgt, src, **kw))
        result.extend(apply(BaseInstallBuilder, (env, tgt, src), kw))
    return result

added = None

def generate(env):

    from SCons.Script import AddOption, GetOption
    global added
    if not added:
        added = 1
        AddOption('--install-sandbox',
                  dest='install_sandbox',
                  type="string",
                  action="store",
                  help='A directory under which all installed files will be placed.')

    global BaseInstallBuilder
    if BaseInstallBuilder is None:
        install_sandbox = GetOption('install_sandbox')
        if install_sandbox:
            target_factory = DESTDIR_factory(env, install_sandbox)
        else:
            target_factory = env.fs

        BaseInstallBuilder = SCons.Builder.Builder(
                              action         = install_action,
                              target_factory = target_factory.Entry,
                              source_factory = env.fs.Entry,
                              multi          = 1,
                              emitter        = [ add_targets_to_INSTALLED_FILES, ],
                              name           = 'InstallBuilder')

    env['BUILDERS']['_InternalInstall'] = InstallBuilderWrapper
    env['BUILDERS']['_InternalInstallAs'] = InstallAsBuilderWrapper

    # We'd like to initialize this doing something like the following,
    # but there isn't yet support for a ${SOURCE.type} expansion that
    # will print "file" or "directory" depending on what's being
    # installed.  For now we punt by not initializing it, and letting
    # the stringFunc() that we put in the action fall back to the
    # hand-crafted default string if it's not set.
    #
    #try:
    #    env['INSTALLSTR']
    #except KeyError:
    #    env['INSTALLSTR'] = 'Install ${SOURCE.type}: "$SOURCES" as "$TARGETS"'

    try:
        env['INSTALL']
    except KeyError:
        env['INSTALL']    = copyFunc

def exists(env):
    return 1
