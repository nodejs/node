"""SCons.Tool.rpm

Tool-specific initialization for rpm.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.

The rpm tool calls the rpmbuild command. The first and only argument should a
tar.gz consisting of the source file and a specfile.
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

__revision__ = "src/engine/SCons/Tool/rpm.py 3842 2008/12/20 22:59:52 scons"

import os
import re
import shutil
import subprocess

import SCons.Builder
import SCons.Node.FS
import SCons.Util
import SCons.Action
import SCons.Defaults

def get_cmd(source, env):
    tar_file_with_included_specfile = source
    if SCons.Util.is_List(source):
        tar_file_with_included_specfile = source[0]
    return "%s %s %s"%(env['RPM'], env['RPMFLAGS'],
                       tar_file_with_included_specfile.abspath )

def build_rpm(target, source, env):
    # create a temporary rpm build root.
    tmpdir = os.path.join( os.path.dirname( target[0].abspath ), 'rpmtemp' )
    if os.path.exists(tmpdir):
        shutil.rmtree(tmpdir)

    # now create the mandatory rpm directory structure.
    for d in ['RPMS', 'SRPMS', 'SPECS', 'BUILD']:
        os.makedirs( os.path.join( tmpdir, d ) )

    # set the topdir as an rpmflag.
    env.Prepend( RPMFLAGS = '--define \'_topdir %s\'' % tmpdir )

    # now call rpmbuild to create the rpm package.
    handle  = subprocess.Popen(get_cmd(source, env),
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT,
                               shell=True)
    output = handle.stdout.read()
    status = handle.wait()

    if status:
        raise SCons.Errors.BuildError( node=target[0],
                                       errstr=output,
                                       filename=str(target[0]) )
    else:
        # XXX: assume that LC_ALL=c is set while running rpmbuild
        output_files = re.compile( 'Wrote: (.*)' ).findall( output )

        for output, input in zip( output_files, target ):
            rpm_output = os.path.basename(output)
            expected   = os.path.basename(input.get_path())

            assert expected == rpm_output, "got %s but expected %s" % (rpm_output, expected)
            shutil.copy( output, input.abspath )


    # cleanup before leaving.
    shutil.rmtree(tmpdir)

    return status

def string_rpm(target, source, env):
    try:
        return env['RPMCOMSTR']
    except KeyError:
        return get_cmd(source, env)

rpmAction = SCons.Action.Action(build_rpm, string_rpm)

RpmBuilder = SCons.Builder.Builder(action = SCons.Action.Action('$RPMCOM', '$RPMCOMSTR'),
                                   source_scanner = SCons.Defaults.DirScanner,
                                   suffix = '$RPMSUFFIX')



def generate(env):
    """Add Builders and construction variables for rpm to an Environment."""
    try:
        bld = env['BUILDERS']['Rpm']
    except KeyError:
        bld = RpmBuilder
        env['BUILDERS']['Rpm'] = bld

    env.SetDefault(RPM          = 'LC_ALL=c rpmbuild')
    env.SetDefault(RPMFLAGS     = SCons.Util.CLVar('-ta'))
    env.SetDefault(RPMCOM       = rpmAction)
    env.SetDefault(RPMSUFFIX    = '.rpm')

def exists(env):
    return env.Detect('rpmbuild')
