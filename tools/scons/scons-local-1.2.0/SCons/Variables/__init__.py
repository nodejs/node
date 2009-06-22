"""engine.SCons.Variables

This file defines the Variables class that is used to add user-friendly
customizable variables to an SCons build.
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

__revision__ = "src/engine/SCons/Variables/__init__.py 3842 2008/12/20 22:59:52 scons"

import os.path
import string
import sys

import SCons.Environment
import SCons.Errors
import SCons.Util
import SCons.Warnings

from BoolVariable import BoolVariable  # okay
from EnumVariable import EnumVariable  # okay
from ListVariable import ListVariable  # naja
from PackageVariable import PackageVariable # naja
from PathVariable import PathVariable # okay


class Variables:
    instance=None

    """
    Holds all the options, updates the environment with the variables,
    and renders the help text.
    """
    def __init__(self, files=[], args={}, is_global=1):
        """
        files - [optional] List of option configuration files to load
            (backward compatibility) If a single string is passed it is
                                     automatically placed in a file list
        """
        self.options = []
        self.args = args
        if not SCons.Util.is_List(files):
            if files:
                files = [ files ]
            else:
                files = []
        self.files = files
        self.unknown = {}

        # create the singleton instance
        if is_global:
            self=Variables.instance

            if not Variables.instance:
                Variables.instance=self

    def _do_add(self, key, help="", default=None, validator=None, converter=None):
        class Variable:
            pass

        option = Variable()

        # if we get a list or a tuple, we take the first element as the
        # option key and store the remaining in aliases.
        if SCons.Util.is_List(key) or SCons.Util.is_Tuple(key):
          option.key     = key[0]
          option.aliases = key[1:]
        else:
          option.key     = key
          option.aliases = [ key ]
        option.help = help
        option.default = default
        option.validator = validator
        option.converter = converter

        self.options.append(option)

    def keys(self):
        """
        Returns the keywords for the options
        """
        return map(lambda o: o.key, self.options)

    def Add(self, key, help="", default=None, validator=None, converter=None, **kw):
        """
        Add an option.

        key - the name of the variable, or a list or tuple of arguments
        help - optional help text for the options
        default - optional default value
        validator - optional function that is called to validate the option's value
                    Called with (key, value, environment)
        converter - optional function that is called to convert the option's value before
                    putting it in the environment.
        """

        if SCons.Util.is_List(key) or type(key) == type(()):
            apply(self._do_add, key)
            return

        if not SCons.Util.is_String(key) or \
           not SCons.Environment.is_valid_construction_var(key):
            raise SCons.Errors.UserError, "Illegal Variables.Add() key `%s'" % str(key)

        self._do_add(key, help, default, validator, converter)

    def AddVariables(self, *optlist):
        """
        Add a list of options.

        Each list element is a tuple/list of arguments to be passed on
        to the underlying method for adding options.

        Example:
          opt.AddVariables(
            ('debug', '', 0),
            ('CC', 'The C compiler'),
            ('VALIDATE', 'An option for testing validation', 'notset',
             validator, None),
            )
        """
        for o in optlist:
            apply(self._do_add, o)


    def Update(self, env, args=None):
        """
        Update an environment with the option variables.

        env - the environment to update.
        """

        values = {}

        # first set the defaults:
        for option in self.options:
            if not option.default is None:
                values[option.key] = option.default

        # next set the value specified in the options file
        for filename in self.files:
            if os.path.exists(filename):
                dir = os.path.split(os.path.abspath(filename))[0]
                if dir:
                    sys.path.insert(0, dir)
                try:
                    values['__name__'] = filename
                    execfile(filename, {}, values)
                finally:
                    if dir:
                        del sys.path[0]
                    del values['__name__']

        # set the values specified on the command line
        if args is None:
            args = self.args

        for arg, value in args.items():
            added = False
            for option in self.options:
                if arg in option.aliases + [ option.key ]:
                    values[option.key] = value
                    added = True
            if not added:
                self.unknown[arg] = value

        # put the variables in the environment:
        # (don't copy over variables that are not declared as options)
        for option in self.options:
            try:
                env[option.key] = values[option.key]
            except KeyError:
                pass

        # Call the convert functions:
        for option in self.options:
            if option.converter and values.has_key(option.key):
                value = env.subst('${%s}'%option.key)
                try:
                    try:
                        env[option.key] = option.converter(value)
                    except TypeError:
                        env[option.key] = option.converter(value, env)
                except ValueError, x:
                    raise SCons.Errors.UserError, 'Error converting option: %s\n%s'%(option.key, x)


        # Finally validate the values:
        for option in self.options:
            if option.validator and values.has_key(option.key):
                option.validator(option.key, env.subst('${%s}'%option.key), env)

    def UnknownVariables(self):
        """
        Returns any options in the specified arguments lists that
        were not known, declared options in this object.
        """
        return self.unknown

    def Save(self, filename, env):
        """
        Saves all the options in the given file.  This file can
        then be used to load the options next run.  This can be used
        to create an option cache file.

        filename - Name of the file to save into
        env - the environment get the option values from
        """

        # Create the file and write out the header
        try:
            fh = open(filename, 'w')

            try:
                # Make an assignment in the file for each option
                # within the environment that was assigned a value
                # other than the default.
                for option in self.options:
                    try:
                        value = env[option.key]
                        try:
                            prepare = value.prepare_to_store
                        except AttributeError:
                            try:
                                eval(repr(value))
                            except KeyboardInterrupt:
                                raise
                            except:
                                # Convert stuff that has a repr() that
                                # cannot be evaluated into a string
                                value = SCons.Util.to_String(value)
                        else:
                            value = prepare()

                        defaultVal = env.subst(SCons.Util.to_String(option.default))
                        if option.converter:
                            defaultVal = option.converter(defaultVal)

                        if str(env.subst('${%s}' % option.key)) != str(defaultVal):
                            fh.write('%s = %s\n' % (option.key, repr(value)))
                    except KeyError:
                        pass
            finally:
                fh.close()

        except IOError, x:
            raise SCons.Errors.UserError, 'Error writing options to file: %s\n%s' % (filename, x)

    def GenerateHelpText(self, env, sort=None):
        """
        Generate the help text for the options.

        env - an environment that is used to get the current values
              of the options.
        """

        if sort:
            options = self.options[:]
            options.sort(lambda x,y,func=sort: func(x.key,y.key))
        else:
            options = self.options

        def format(opt, self=self, env=env):
            if env.has_key(opt.key):
                actual = env.subst('${%s}' % opt.key)
            else:
                actual = None
            return self.FormatVariableHelpText(env, opt.key, opt.help, opt.default, actual, opt.aliases)
        lines = filter(None, map(format, options))

        return string.join(lines, '')

    format  = '\n%s: %s\n    default: %s\n    actual: %s\n'
    format_ = '\n%s: %s\n    default: %s\n    actual: %s\n    aliases: %s\n'

    def FormatVariableHelpText(self, env, key, help, default, actual, aliases=[]):
        # Don't display the key name itself as an alias.
        aliases = filter(lambda a, k=key: a != k, aliases)
        if len(aliases)==0:
            return self.format % (key, help, default, actual)
        else:
            return self.format_ % (key, help, default, actual, aliases)

