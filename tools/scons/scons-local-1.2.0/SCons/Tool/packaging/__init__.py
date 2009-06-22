"""SCons.Tool.Packaging

SCons Packaging Tool.
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

__revision__ = "src/engine/SCons/Tool/packaging/__init__.py 3842 2008/12/20 22:59:52 scons"

import SCons.Environment
from SCons.Variables import *
from SCons.Errors import *
from SCons.Util import is_List, make_path_relative
from SCons.Warnings import warn, Warning

import os, imp
import SCons.Defaults

__all__ = [ 'src_targz', 'src_tarbz2', 'src_zip', 'tarbz2', 'targz', 'zip', 'rpm', 'msi', 'ipk' ]

#
# Utility and Builder function
#
def Tag(env, target, source, *more_tags, **kw_tags):
    """ Tag a file with the given arguments, just sets the accordingly named
    attribute on the file object.

    TODO: FIXME
    """
    if not target:
        target=source
        first_tag=None
    else:
        first_tag=source

    if first_tag:
        kw_tags[first_tag[0]] = ''

    if len(kw_tags) == 0 and len(more_tags) == 0:
        raise UserError, "No tags given."

    # XXX: sanity checks
    for x in more_tags:
        kw_tags[x] = ''

    if not SCons.Util.is_List(target):
        target=[target]
    else:
        # hmm, sometimes the target list, is a list of a list
        # make sure it is flattened prior to processing.
        # TODO: perhaps some bug ?!?
        target=env.Flatten(target)

    for t in target:
        for (k,v) in kw_tags.items():
            # all file tags have to start with PACKAGING_, so we can later
            # differentiate between "normal" object attributes and the
            # packaging attributes. As the user should not be bothered with
            # that, the prefix will be added here if missing.
            #if not k.startswith('PACKAGING_'):
            if k[:10] != 'PACKAGING_':
                k='PACKAGING_'+k
            setattr(t, k, v)

def Package(env, target=None, source=None, **kw):
    """ Entry point for the package tool.
    """
    # check if we need to find the source files ourself
    if not source:
        source = env.FindInstalledFiles()

    if len(source)==0:
        raise UserError, "No source for Package() given"

    # decide which types of packages shall be built. Can be defined through
    # four mechanisms: command line argument, keyword argument,
    # environment argument and default selection( zip or tar.gz ) in that
    # order.
    try: kw['PACKAGETYPE']=env['PACKAGETYPE']
    except KeyError: pass

    if not kw.get('PACKAGETYPE'):
        from SCons.Script import GetOption
        kw['PACKAGETYPE'] = GetOption('package_type')

    if kw['PACKAGETYPE'] == None:
        if env['BUILDERS'].has_key('Tar'):
            kw['PACKAGETYPE']='targz'
        elif env['BUILDERS'].has_key('Zip'):
            kw['PACKAGETYPE']='zip'
        else:
            raise UserError, "No type for Package() given"

    PACKAGETYPE=kw['PACKAGETYPE']
    if not is_List(PACKAGETYPE):
        PACKAGETYPE=string.split(PACKAGETYPE, ',')

    # load the needed packagers.
    def load_packager(type):
        try:
            file,path,desc=imp.find_module(type, __path__)
            return imp.load_module(type, file, path, desc)
        except ImportError, e:
            raise EnvironmentError("packager %s not available: %s"%(type,str(e)))

    packagers=map(load_packager, PACKAGETYPE)

    # set up targets and the PACKAGEROOT
    try:
        # fill up the target list with a default target name until the PACKAGETYPE
        # list is of the same size as the target list.
        if not target: target = []

        size_diff      = len(PACKAGETYPE)-len(target)
        default_name   = "%(NAME)s-%(VERSION)s"

        if size_diff>0:
            default_target = default_name%kw
            target.extend( [default_target]*size_diff )

        if not kw.has_key('PACKAGEROOT'):
            kw['PACKAGEROOT'] = default_name%kw

    except KeyError, e:
        raise SCons.Errors.UserError( "Missing Packagetag '%s'"%e.args[0] )

    # setup the source files
    source=env.arg2nodes(source, env.fs.Entry)

    # call the packager to setup the dependencies.
    targets=[]
    try:
        for packager in packagers:
            t=[target.pop(0)]
            t=apply(packager.package, [env,t,source], kw)
            targets.extend(t)

        assert( len(target) == 0 )

    except KeyError, e:
        raise SCons.Errors.UserError( "Missing Packagetag '%s' for %s packager"\
                                      % (e.args[0],packager.__name__) )
    except TypeError, e:
        # this exception means that a needed argument for the packager is
        # missing. As our packagers get their "tags" as named function
        # arguments we need to find out which one is missing.
        from inspect import getargspec
        args,varargs,varkw,defaults=getargspec(packager.package)
        if defaults!=None:
            args=args[:-len(defaults)] # throw away arguments with default values
        map(args.remove, 'env target source'.split())
        # now remove any args for which we have a value in kw.
        #args=[x for x in args if not kw.has_key(x)]
        args=filter(lambda x, kw=kw: not kw.has_key(x), args)

        if len(args)==0:
            raise # must be a different error, so reraise
        elif len(args)==1:
            raise SCons.Errors.UserError( "Missing Packagetag '%s' for %s packager"\
                                          % (args[0],packager.__name__) )
        else:
            raise SCons.Errors.UserError( "Missing Packagetags '%s' for %s packager"\
                                          % (", ".join(args),packager.__name__) )

    target=env.arg2nodes(target, env.fs.Entry)
    targets.extend(env.Alias( 'package', targets ))
    return targets

#
# SCons tool initialization functions
#

added = None

def generate(env):
    from SCons.Script import AddOption
    global added
    if not added:
        added = 1
        AddOption('--package-type',
                  dest='package_type',
                  default=None,
                  type="string",
                  action="store",
                  help='The type of package to create.')

    try:
        env['BUILDERS']['Package']
        env['BUILDERS']['Tag']
    except KeyError:
        env['BUILDERS']['Package'] = Package
        env['BUILDERS']['Tag'] = Tag

def exists(env):
    return 1

# XXX
def options(opts):
    opts.AddVariables(
        EnumVariable( 'PACKAGETYPE',
                     'the type of package to create.',
                     None, allowed_values=map( str, __all__ ),
                     ignorecase=2
                  )
    )

#
# Internal utility functions
#

def copy_attr(f1, f2):
    """ copies the special packaging file attributes from f1 to f2.
    """
    #pattrs = [x for x in dir(f1) if not hasattr(f2, x) and\
    #                                x.startswith('PACKAGING_')]
    copyit = lambda x, f2=f2: not hasattr(f2, x) and x[:10] == 'PACKAGING_'
    pattrs = filter(copyit, dir(f1))
    for attr in pattrs:
        setattr(f2, attr, getattr(f1, attr))
def putintopackageroot(target, source, env, pkgroot, honor_install_location=1):
    """ Uses the CopyAs builder to copy all source files to the directory given
    in pkgroot.

    If honor_install_location is set and the copied source file has an
    PACKAGING_INSTALL_LOCATION attribute, the PACKAGING_INSTALL_LOCATION is
    used as the new name of the source file under pkgroot.

    The source file will not be copied if it is already under the the pkgroot
    directory.

    All attributes of the source file will be copied to the new file.
    """
    # make sure the packageroot is a Dir object.
    if SCons.Util.is_String(pkgroot):  pkgroot=env.Dir(pkgroot)
    if not SCons.Util.is_List(source): source=[source]

    new_source = []
    for file in source:
        if SCons.Util.is_String(file): file = env.File(file)

        if file.is_under(pkgroot):
            new_source.append(file)
        else:
            if hasattr(file, 'PACKAGING_INSTALL_LOCATION') and\
                       honor_install_location:
                new_name=make_path_relative(file.PACKAGING_INSTALL_LOCATION)
            else:
                new_name=make_path_relative(file.get_path())

            new_file=pkgroot.File(new_name)
            new_file=env.CopyAs(new_file, file)[0]
            copy_attr(file, new_file)
            new_source.append(new_file)

    return (target, new_source)

def stripinstallbuilder(target, source, env):
    """ strips the install builder action from the source list and stores
    the final installation location as the "PACKAGING_INSTALL_LOCATION" of
    the source of the source file. This effectively removes the final installed
    files from the source list while remembering the installation location.

    It also warns about files which have no install builder attached.
    """
    def has_no_install_location(file):
        return not (file.has_builder() and\
            hasattr(file.builder, 'name') and\
            (file.builder.name=="InstallBuilder" or\
             file.builder.name=="InstallAsBuilder"))

    if len(filter(has_no_install_location, source)):
        warn(Warning, "there are files to package which have no\
        InstallBuilder attached, this might lead to irreproducible packages")

    n_source=[]
    for s in source:
        if has_no_install_location(s):
            n_source.append(s)
        else:
            for ss in s.sources:
                n_source.append(ss)
                copy_attr(s, ss)
                setattr(ss, 'PACKAGING_INSTALL_LOCATION', s.get_path())

    return (target, n_source)
