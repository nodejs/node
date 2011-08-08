"""SCons.Tool.Packaging.ipk
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

__revision__ = "src/engine/SCons/Tool/packaging/ipk.py 3842 2008/12/20 22:59:52 scons"

import SCons.Builder
import SCons.Node.FS
import os

from SCons.Tool.packaging import stripinstallbuilder, putintopackageroot

def package(env, target, source, PACKAGEROOT, NAME, VERSION, DESCRIPTION,
            SUMMARY, X_IPK_PRIORITY, X_IPK_SECTION, SOURCE_URL,
            X_IPK_MAINTAINER, X_IPK_DEPENDS, **kw):
    """ this function prepares the packageroot directory for packaging with the
    ipkg builder.
    """
    SCons.Tool.Tool('ipkg').generate(env)

    # setup the Ipkg builder
    bld = env['BUILDERS']['Ipkg']
    target, source = stripinstallbuilder(target, source, env)
    target, source = putintopackageroot(target, source, env, PACKAGEROOT)

    # This should be overridable from the construction environment,
    # which it is by using ARCHITECTURE=.
    # Guessing based on what os.uname() returns at least allows it
    # to work for both i386 and x86_64 Linux systems.
    archmap = {
        'i686'  : 'i386',
        'i586'  : 'i386',
        'i486'  : 'i386',
    }

    buildarchitecture = os.uname()[4]
    buildarchitecture = archmap.get(buildarchitecture, buildarchitecture)

    if kw.has_key('ARCHITECTURE'):
        buildarchitecture = kw['ARCHITECTURE']

    # setup the kw to contain the mandatory arguments to this fucntion.
    # do this before calling any builder or setup function
    loc=locals()
    del loc['kw']
    kw.update(loc)
    del kw['source'], kw['target'], kw['env']

    # generate the specfile
    specfile = gen_ipk_dir(PACKAGEROOT, source, env, kw)

    # override the default target.
    if str(target[0])=="%s-%s"%(NAME, VERSION):
        target=[ "%s_%s_%s.ipk"%(NAME, VERSION, buildarchitecture) ]

    # now apply the Ipkg builder
    return apply(bld, [env, target, specfile], kw)

def gen_ipk_dir(proot, source, env, kw):
    # make sure the packageroot is a Dir object.
    if SCons.Util.is_String(proot): proot=env.Dir(proot)

    #  create the specfile builder
    s_bld=SCons.Builder.Builder(
        action  = build_specfiles,
        )

    # create the specfile targets
    spec_target=[]
    control=proot.Dir('CONTROL')
    spec_target.append(control.File('control'))
    spec_target.append(control.File('conffiles'))
    spec_target.append(control.File('postrm'))
    spec_target.append(control.File('prerm'))
    spec_target.append(control.File('postinst'))
    spec_target.append(control.File('preinst'))

    # apply the builder to the specfile targets
    apply(s_bld, [env, spec_target, source], kw)

    # the packageroot directory does now contain the specfiles.
    return proot

def build_specfiles(source, target, env):
    """ filter the targets for the needed files and use the variables in env
    to create the specfile.
    """
    #
    # At first we care for the CONTROL/control file, which is the main file for ipk.
    #
    # For this we need to open multiple files in random order, so we store into
    # a dict so they can be easily accessed.
    #
    #
    opened_files={}
    def open_file(needle, haystack):
        try:
            return opened_files[needle]
        except KeyError:
            file=filter(lambda x: x.get_path().rfind(needle)!=-1, haystack)[0]
            opened_files[needle]=open(file.abspath, 'w')
            return opened_files[needle]

    control_file=open_file('control', target)

    if not env.has_key('X_IPK_DESCRIPTION'):
        env['X_IPK_DESCRIPTION']="%s\n %s"%(env['SUMMARY'],
                                            env['DESCRIPTION'].replace('\n', '\n '))


    content = """
Package: $NAME
Version: $VERSION
Priority: $X_IPK_PRIORITY
Section: $X_IPK_SECTION
Source: $SOURCE_URL
Architecture: $ARCHITECTURE
Maintainer: $X_IPK_MAINTAINER
Depends: $X_IPK_DEPENDS
Description: $X_IPK_DESCRIPTION
"""

    control_file.write(env.subst(content))

    #
    # now handle the various other files, which purpose it is to set post-, 
    # pre-scripts and mark files as config files.
    #
    # We do so by filtering the source files for files which are marked with
    # the "config" tag and afterwards we do the same for x_ipk_postrm,
    # x_ipk_prerm, x_ipk_postinst and x_ipk_preinst tags.
    #
    # The first one will write the name of the file into the file
    # CONTROL/configfiles, the latter add the content of the x_ipk_* variable
    # into the same named file.
    #
    for f in [x for x in source if 'PACKAGING_CONFIG' in dir(x)]:
        config=open_file('conffiles')
        config.write(f.PACKAGING_INSTALL_LOCATION)
        config.write('\n')

    for str in 'POSTRM PRERM POSTINST PREINST'.split():
        name="PACKAGING_X_IPK_%s"%str
        for f in [x for x in source if name in dir(x)]:
            file=open_file(name)
            file.write(env[str])

    #
    # close all opened files
    for f in opened_files.values():
        f.close()

    # call a user specified function
    if env.has_key('CHANGE_SPECFILE'):
        content += env['CHANGE_SPECFILE'](target)

    return 0
