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

"""SCons.Errors

This file contains the exception classes used to handle internal
and user errors in SCons.

"""

__revision__ = "src/engine/SCons/Errors.py 3842 2008/12/20 22:59:52 scons"

import SCons.Util

import exceptions

class BuildError(Exception):
    """ Errors occuring while building.

    BuildError have the following attributes:

        Information about the cause of the build error:
        -----------------------------------------------

        errstr : a description of the error message

        status : the return code of the action that caused the build
                 error. Must be set to a non-zero value even if the
                 build error is not due to an action returning a
                 non-zero returned code.

        exitstatus : SCons exit status due to this build error.
                     Must be nonzero unless due to an explicit Exit()
                     call.  Not always the same as status, since
                     actions return a status code that should be
                     respected, but SCons typically exits with 2
                     irrespective of the return value of the failed
                     action.

        filename : The name of the file or directory that caused the
                   build error. Set to None if no files are associated with
                   this error. This might be different from the target
                   being built. For example, failure to create the
                   directory in which the target file will appear. It
                   can be None if the error is not due to a particular
                   filename.

        exc_info : Info about exception that caused the build
                   error. Set to (None, None, None) if this build
                   error is not due to an exception.


        Information about the cause of the location of the error:
        ---------------------------------------------------------

        node : the error occured while building this target node(s)
        
        executor : the executor that caused the build to fail (might
                   be None if the build failures is not due to the
                   executor failing)
        
        action : the action that caused the build to fail (might be
                 None if the build failures is not due to the an
                 action failure)

        command : the command line for the action that caused the
                  build to fail (might be None if the build failures
                  is not due to the an action failure)
        """

    def __init__(self, 
                 node=None, errstr="Unknown error", status=2, exitstatus=2,
                 filename=None, executor=None, action=None, command=None,
                 exc_info=(None, None, None)):
        
        self.errstr = errstr
        self.status = status
        self.exitstatus = exitstatus
        self.filename = filename
        self.exc_info = exc_info

        self.node = node
        self.executor = executor
        self.action = action
        self.command = command

        Exception.__init__(self, node, errstr, status, exitstatus, filename, 
                           executor, action, command, exc_info)

    def __str__(self):
        if self.filename:
            return self.filename + ': ' + self.errstr
        else:
            return self.errstr

class InternalError(Exception):
    pass

class UserError(Exception):
    pass

class StopError(Exception):
    pass

class EnvironmentError(Exception):
    pass

class ExplicitExit(Exception):
    def __init__(self, node=None, status=None, *args):
        self.node = node
        self.status = status
        self.exitstatus = status
        apply(Exception.__init__, (self,) + args)

def convert_to_BuildError(status, exc_info=None):
    """
    Convert any return code a BuildError Exception.

    `status' can either be a return code or an Exception.
    The buildError.status we set here will normally be
    used as the exit status of the "scons" process.
    """
    if not exc_info and isinstance(status, Exception):
        exc_info = (status.__class__, status, None)

    if isinstance(status, BuildError):
        buildError = status
        buildError.exitstatus = 2   # always exit with 2 on build errors
    elif isinstance(status, ExplicitExit):
        status = status.status
        errstr = 'Explicit exit, status %s' % status
        buildError = BuildError(
            errstr=errstr,
            status=status,      # might be 0, OK here
            exitstatus=status,      # might be 0, OK here
            exc_info=exc_info)
    # TODO(1.5):
    #elif isinstance(status, (StopError, UserError)):
    elif isinstance(status, StopError) or isinstance(status, UserError):
        buildError = BuildError(
            errstr=str(status),
            status=2,
            exitstatus=2,
            exc_info=exc_info)
    elif isinstance(status, exceptions.EnvironmentError):
        # If an IOError/OSError happens, raise a BuildError.
        # Report the name of the file or directory that caused the
        # error, which might be different from the target being built
        # (for example, failure to create the directory in which the
        # target file will appear).
        try: filename = status.filename
        except AttributeError: filename = None
        buildError = BuildError( 
            errstr=status.strerror,
            status=status.errno,
            exitstatus=2,
            filename=filename,
            exc_info=exc_info)
    elif isinstance(status, Exception):
        buildError = BuildError(
            errstr='%s : %s' % (status.__class__.__name__, status),
            status=2,
            exitstatus=2,
            exc_info=exc_info)
    elif SCons.Util.is_String(status):
        buildError = BuildError(
            errstr=status,
            status=2,
            exitstatus=2)
    else:
        buildError = BuildError(
            errstr="Error %s" % status,
            status=status,
            exitstatus=2)
    
    #import sys
    #sys.stderr.write("convert_to_BuildError: status %s => (errstr %s, status %s)"%(status,buildError.errstr, buildError.status))
    return buildError
