#!/usr/bin/env python
#
# Copyright (c) 2002, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ---
# Author: Chad Lester
# Design and style contributions by:
#   Amit Patel, Bogdan Cocosel, Daniel Dulitz, Eric Tiedemann,
#   Eric Veach, Laurence Gonsalves, Matthew Springer
# Code reorganized a bit by Craig Silverstein

"""This module is used to define and parse command line flags.

This module defines a *distributed* flag-definition policy: rather than
an application having to define all flags in or near main(), each python
module defines flags that are useful to it.  When one python module
imports another, it gains access to the other's flags.  (This is
implemented by having all modules share a common, global registry object
containing all the flag information.)

Flags are defined through the use of one of the DEFINE_xxx functions.
The specific function used determines how the flag is parsed, checked,
and optionally type-converted, when it's seen on the command line.


IMPLEMENTATION: DEFINE_* creates a 'Flag' object and registers it with a
'FlagValues' object (typically the global FlagValues FLAGS, defined
here).  The 'FlagValues' object can scan the command line arguments and
pass flag arguments to the corresponding 'Flag' objects for
value-checking and type conversion.  The converted flag values are
available as attributes of the 'FlagValues' object.

Code can access the flag through a FlagValues object, for instance
gflags.FLAGS.myflag.  Typically, the __main__ module passes the command
line arguments to gflags.FLAGS for parsing.

At bottom, this module calls getopt(), so getopt functionality is
supported, including short- and long-style flags, and the use of -- to
terminate flags.

Methods defined by the flag module will throw 'FlagsError' exceptions.
The exception argument will be a human-readable string.


FLAG TYPES: This is a list of the DEFINE_*'s that you can do.  All flags
take a name, default value, help-string, and optional 'short' name
(one-letter name).  Some flags have other arguments, which are described
with the flag.

DEFINE_string: takes any input, and interprets it as a string.

DEFINE_bool or
DEFINE_boolean: typically does not take an argument: say --myflag to
                set FLAGS.myflag to true, or --nomyflag to set
                FLAGS.myflag to false.  Alternately, you can say
                   --myflag=true  or --myflag=t or --myflag=1  or
                   --myflag=false or --myflag=f or --myflag=0

DEFINE_float: takes an input and interprets it as a floating point
              number.  Takes optional args lower_bound and upper_bound;
              if the number specified on the command line is out of
              range, it will raise a FlagError.

DEFINE_integer: takes an input and interprets it as an integer.  Takes
                optional args lower_bound and upper_bound as for floats.

DEFINE_enum: takes a list of strings which represents legal values.  If
             the command-line value is not in this list, raise a flag
             error.  Otherwise, assign to FLAGS.flag as a string.

DEFINE_list: Takes a comma-separated list of strings on the commandline.
             Stores them in a python list object.

DEFINE_spaceseplist: Takes a space-separated list of strings on the
                     commandline.  Stores them in a python list object.
                     Example: --myspacesepflag "foo bar baz"

DEFINE_multistring: The same as DEFINE_string, except the flag can be
                    specified more than once on the commandline.  The
                    result is a python list object (list of strings),
                    even if the flag is only on the command line once.

DEFINE_multi_int: The same as DEFINE_integer, except the flag can be
                  specified more than once on the commandline.  The
                  result is a python list object (list of ints), even if
                  the flag is only on the command line once.


SPECIAL FLAGS: There are a few flags that have special meaning:
   --help          prints a list of all the flags in a human-readable fashion
   --helpshort     prints a list of all key flags (see below).
   --helpxml       prints a list of all flags, in XML format.  DO NOT parse
                   the output of --help and --helpshort.  Instead, parse
                   the output of --helpxml.  For more info, see
                   "OUTPUT FOR --helpxml" below.
   --flagfile=foo  read flags from file foo.
   --undefok=f1,f2 ignore unrecognized option errors for f1,f2.
                   For boolean flags, you should use --undefok=boolflag, and
                   --boolflag and --noboolflag will be accepted.  Do not use
                   --undefok=noboolflag.
   --              as in getopt(), terminates flag-processing


FLAGS VALIDATORS: If your program:
  - requires flag X to be specified
  - needs flag Y to match a regular expression
  - or requires any more general constraint to be satisfied
then validators are for you!

Each validator represents a constraint over one flag, which is enforced
starting from the initial parsing of the flags and until the program
terminates.

Also, lower_bound and upper_bound for numerical flags are enforced using flag
validators.

Howto:
If you want to enforce a constraint over one flag, use

gflags.RegisterValidator(flag_name,
                        checker,
                        message='Flag validation failed',
                        flag_values=FLAGS)

After flag values are initially parsed, and after any change to the specified
flag, method checker(flag_value) will be executed. If constraint is not
satisfied, an IllegalFlagValue exception will be raised. See
RegisterValidator's docstring for a detailed explanation on how to construct
your own checker.


EXAMPLE USAGE:

FLAGS = gflags.FLAGS

gflags.DEFINE_integer('my_version', 0, 'Version number.')
gflags.DEFINE_string('filename', None, 'Input file name', short_name='f')

gflags.RegisterValidator('my_version',
                        lambda value: value % 2 == 0,
                        message='--my_version must be divisible by 2')
gflags.MarkFlagAsRequired('filename')


NOTE ON --flagfile:

Flags may be loaded from text files in addition to being specified on
the commandline.

Any flags you don't feel like typing, throw them in a file, one flag per
line, for instance:
   --myflag=myvalue
   --nomyboolean_flag
You then specify your file with the special flag '--flagfile=somefile'.
You CAN recursively nest flagfile= tokens OR use multiple files on the
command line.  Lines beginning with a single hash '#' or a double slash
'//' are comments in your flagfile.

Any flagfile=<file> will be interpreted as having a relative path from
the current working directory rather than from the place the file was
included from:
   myPythonScript.py --flagfile=config/somefile.cfg

If somefile.cfg includes further --flagfile= directives, these will be
referenced relative to the original CWD, not from the directory the
including flagfile was found in!

The caveat applies to people who are including a series of nested files
in a different dir than they are executing out of.  Relative path names
are always from CWD, not from the directory of the parent include
flagfile. We do now support '~' expanded directory names.

Absolute path names ALWAYS work!


EXAMPLE USAGE:


  FLAGS = gflags.FLAGS

  # Flag names are globally defined!  So in general, we need to be
  # careful to pick names that are unlikely to be used by other libraries.
  # If there is a conflict, we'll get an error at import time.
  gflags.DEFINE_string('name', 'Mr. President', 'your name')
  gflags.DEFINE_integer('age', None, 'your age in years', lower_bound=0)
  gflags.DEFINE_boolean('debug', False, 'produces debugging output')
  gflags.DEFINE_enum('gender', 'male', ['male', 'female'], 'your gender')

  def main(argv):
    try:
      argv = FLAGS(argv)  # parse flags
    except gflags.FlagsError, e:
      print '%s\\nUsage: %s ARGS\\n%s' % (e, sys.argv[0], FLAGS)
      sys.exit(1)
    if FLAGS.debug: print 'non-flag arguments:', argv
    print 'Happy Birthday', FLAGS.name
    if FLAGS.age is not None:
      print 'You are a %d year old %s' % (FLAGS.age, FLAGS.gender)

  if __name__ == '__main__':
    main(sys.argv)


KEY FLAGS:

As we already explained, each module gains access to all flags defined
by all the other modules it transitively imports.  In the case of
non-trivial scripts, this means a lot of flags ...  For documentation
purposes, it is good to identify the flags that are key (i.e., really
important) to a module.  Clearly, the concept of "key flag" is a
subjective one.  When trying to determine whether a flag is key to a
module or not, assume that you are trying to explain your module to a
potential user: which flags would you really like to mention first?

We'll describe shortly how to declare which flags are key to a module.
For the moment, assume we know the set of key flags for each module.
Then, if you use the app.py module, you can use the --helpshort flag to
print only the help for the flags that are key to the main module, in a
human-readable format.

NOTE: If you need to parse the flag help, do NOT use the output of
--help / --helpshort.  That output is meant for human consumption, and
may be changed in the future.  Instead, use --helpxml; flags that are
key for the main module are marked there with a <key>yes</key> element.

The set of key flags for a module M is composed of:

1. Flags defined by module M by calling a DEFINE_* function.

2. Flags that module M explictly declares as key by using the function

     DECLARE_key_flag(<flag_name>)

3. Key flags of other modules that M specifies by using the function

     ADOPT_module_key_flags(<other_module>)

   This is a "bulk" declaration of key flags: each flag that is key for
   <other_module> becomes key for the current module too.

Notice that if you do not use the functions described at points 2 and 3
above, then --helpshort prints information only about the flags defined
by the main module of our script.  In many cases, this behavior is good
enough.  But if you move part of the main module code (together with the
related flags) into a different module, then it is nice to use
DECLARE_key_flag / ADOPT_module_key_flags and make sure --helpshort
lists all relevant flags (otherwise, your code refactoring may confuse
your users).

Note: each of DECLARE_key_flag / ADOPT_module_key_flags has its own
pluses and minuses: DECLARE_key_flag is more targeted and may lead a
more focused --helpshort documentation.  ADOPT_module_key_flags is good
for cases when an entire module is considered key to the current script.
Also, it does not require updates to client scripts when a new flag is
added to the module.


EXAMPLE USAGE 2 (WITH KEY FLAGS):

Consider an application that contains the following three files (two
auxiliary modules and a main module)

File libfoo.py:

  import gflags

  gflags.DEFINE_integer('num_replicas', 3, 'Number of replicas to start')
  gflags.DEFINE_boolean('rpc2', True, 'Turn on the usage of RPC2.')

  ... some code ...

File libbar.py:

  import gflags

  gflags.DEFINE_string('bar_gfs_path', '/gfs/path',
                      'Path to the GFS files for libbar.')
  gflags.DEFINE_string('email_for_bar_errors', 'bar-team@google.com',
                      'Email address for bug reports about module libbar.')
  gflags.DEFINE_boolean('bar_risky_hack', False,
                       'Turn on an experimental and buggy optimization.')

  ... some code ...

File myscript.py:

  import gflags
  import libfoo
  import libbar

  gflags.DEFINE_integer('num_iterations', 0, 'Number of iterations.')

  # Declare that all flags that are key for libfoo are
  # key for this module too.
  gflags.ADOPT_module_key_flags(libfoo)

  # Declare that the flag --bar_gfs_path (defined in libbar) is key
  # for this module.
  gflags.DECLARE_key_flag('bar_gfs_path')

  ... some code ...

When myscript is invoked with the flag --helpshort, the resulted help
message lists information about all the key flags for myscript:
--num_iterations, --num_replicas, --rpc2, and --bar_gfs_path.

Of course, myscript uses all the flags declared by it (in this case,
just --num_replicas) or by any of the modules it transitively imports
(e.g., the modules libfoo, libbar).  E.g., it can access the value of
FLAGS.bar_risky_hack, even if --bar_risky_hack is not declared as a key
flag for myscript.


OUTPUT FOR --helpxml:

The --helpxml flag generates output with the following structure:

<?xml version="1.0"?>
<AllFlags>
  <program>PROGRAM_BASENAME</program>
  <usage>MAIN_MODULE_DOCSTRING</usage>
  (<flag>
    [<key>yes</key>]
    <file>DECLARING_MODULE</file>
    <name>FLAG_NAME</name>
    <meaning>FLAG_HELP_MESSAGE</meaning>
    <default>DEFAULT_FLAG_VALUE</default>
    <current>CURRENT_FLAG_VALUE</current>
    <type>FLAG_TYPE</type>
    [OPTIONAL_ELEMENTS]
  </flag>)*
</AllFlags>

Notes:

1. The output is intentionally similar to the output generated by the
C++ command-line flag library.  The few differences are due to the
Python flags that do not have a C++ equivalent (at least not yet),
e.g., DEFINE_list.

2. New XML elements may be added in the future.

3. DEFAULT_FLAG_VALUE is in serialized form, i.e., the string you can
pass for this flag on the command-line.  E.g., for a flag defined
using DEFINE_list, this field may be foo,bar, not ['foo', 'bar'].

4. CURRENT_FLAG_VALUE is produced using str().  This means that the
string 'false' will be represented in the same way as the boolean
False.  Using repr() would have removed this ambiguity and simplified
parsing, but would have broken the compatibility with the C++
command-line flags.

5. OPTIONAL_ELEMENTS describe elements relevant for certain kinds of
flags: lower_bound, upper_bound (for flags that specify bounds),
enum_value (for enum flags), list_separator (for flags that consist of
a list of values, separated by a special token).

6. We do not provide any example here: please use --helpxml instead.

This module requires at least python 2.2.1 to run.
"""

import cgi
import getopt
import os
import re
import string
import struct
import sys
# pylint: disable-msg=C6204
try:
  import fcntl
except ImportError:
  fcntl = None
try:
  # Importing termios will fail on non-unix platforms.
  import termios
except ImportError:
  termios = None

import gflags_validators
# pylint: enable-msg=C6204


# Are we running under pychecker?
_RUNNING_PYCHECKER = 'pychecker.python' in sys.modules


def _GetCallingModuleObjectAndName():
  """Returns the module that's calling into this module.

  We generally use this function to get the name of the module calling a
  DEFINE_foo... function.
  """
  # Walk down the stack to find the first globals dict that's not ours.
  for depth in range(1, sys.getrecursionlimit()):
    if not sys._getframe(depth).f_globals is globals():
      globals_for_frame = sys._getframe(depth).f_globals
      module, module_name = _GetModuleObjectAndName(globals_for_frame)
      if module_name is not None:
        return module, module_name
  raise AssertionError("No module was found")


def _GetCallingModule():
  """Returns the name of the module that's calling into this module."""
  return _GetCallingModuleObjectAndName()[1]


def _GetThisModuleObjectAndName():
  """Returns: (module object, module name) for this module."""
  return _GetModuleObjectAndName(globals())


# module exceptions:
class FlagsError(Exception):
  """The base class for all flags errors."""
  pass


class DuplicateFlag(FlagsError):
  """Raised if there is a flag naming conflict."""
  pass

class CantOpenFlagFileError(FlagsError):
  """Raised if flagfile fails to open: doesn't exist, wrong permissions, etc."""
  pass


class DuplicateFlagCannotPropagateNoneToSwig(DuplicateFlag):
  """Special case of DuplicateFlag -- SWIG flag value can't be set to None.

  This can be raised when a duplicate flag is created. Even if allow_override is
  True, we still abort if the new value is None, because it's currently
  impossible to pass None default value back to SWIG. See FlagValues.SetDefault
  for details.
  """
  pass


class DuplicateFlagError(DuplicateFlag):
  """A DuplicateFlag whose message cites the conflicting definitions.

  A DuplicateFlagError conveys more information than a DuplicateFlag,
  namely the modules where the conflicting definitions occur. This
  class was created to avoid breaking external modules which depend on
  the existing DuplicateFlags interface.
  """

  def __init__(self, flagname, flag_values, other_flag_values=None):
    """Create a DuplicateFlagError.

    Args:
      flagname: Name of the flag being redefined.
      flag_values: FlagValues object containing the first definition of
          flagname.
      other_flag_values: If this argument is not None, it should be the
          FlagValues object where the second definition of flagname occurs.
          If it is None, we assume that we're being called when attempting
          to create the flag a second time, and we use the module calling
          this one as the source of the second definition.
    """
    self.flagname = flagname
    first_module = flag_values.FindModuleDefiningFlag(
        flagname, default='<unknown>')
    if other_flag_values is None:
      second_module = _GetCallingModule()
    else:
      second_module = other_flag_values.FindModuleDefiningFlag(
          flagname, default='<unknown>')
    msg = "The flag '%s' is defined twice. First from %s, Second from %s" % (
        self.flagname, first_module, second_module)
    DuplicateFlag.__init__(self, msg)


class IllegalFlagValue(FlagsError):
  """The flag command line argument is illegal."""
  pass


class UnrecognizedFlag(FlagsError):
  """Raised if a flag is unrecognized."""
  pass


# An UnrecognizedFlagError conveys more information than an UnrecognizedFlag.
# Since there are external modules that create DuplicateFlags, the interface to
# DuplicateFlag shouldn't change.  The flagvalue will be assigned the full value
# of the flag and its argument, if any, allowing handling of unrecognized flags
# in an exception handler.
# If flagvalue is the empty string, then this exception is an due to a
# reference to a flag that was not already defined.
class UnrecognizedFlagError(UnrecognizedFlag):
  def __init__(self, flagname, flagvalue=''):
    self.flagname = flagname
    self.flagvalue = flagvalue
    UnrecognizedFlag.__init__(
        self, "Unknown command line flag '%s'" % flagname)

# Global variable used by expvar
_exported_flags = {}
_help_width = 80  # width of help output


def GetHelpWidth():
  """Returns: an integer, the width of help lines that is used in TextWrap."""
  if (not sys.stdout.isatty()) or (termios is None) or (fcntl is None):
    return _help_width
  try:
    data = fcntl.ioctl(sys.stdout, termios.TIOCGWINSZ, '1234')
    columns = struct.unpack('hh', data)[1]
    # Emacs mode returns 0.
    # Here we assume that any value below 40 is unreasonable
    if columns >= 40:
      return columns
    # Returning an int as default is fine, int(int) just return the int.
    return int(os.getenv('COLUMNS', _help_width))

  except (TypeError, IOError, struct.error):
    return _help_width


def CutCommonSpacePrefix(text):
  """Removes a common space prefix from the lines of a multiline text.

  If the first line does not start with a space, it is left as it is and
  only in the remaining lines a common space prefix is being searched
  for. That means the first line will stay untouched. This is especially
  useful to turn doc strings into help texts. This is because some
  people prefer to have the doc comment start already after the
  apostrophe and then align the following lines while others have the
  apostrophes on a separate line.

  The function also drops trailing empty lines and ignores empty lines
  following the initial content line while calculating the initial
  common whitespace.

  Args:
    text: text to work on

  Returns:
    the resulting text
  """
  text_lines = text.splitlines()
  # Drop trailing empty lines
  while text_lines and not text_lines[-1]:
    text_lines = text_lines[:-1]
  if text_lines:
    # We got some content, is the first line starting with a space?
    if text_lines[0] and text_lines[0][0].isspace():
      text_first_line = []
    else:
      text_first_line = [text_lines.pop(0)]
    # Calculate length of common leading whitespace (only over content lines)
    common_prefix = os.path.commonprefix([line for line in text_lines if line])
    space_prefix_len = len(common_prefix) - len(common_prefix.lstrip())
    # If we have a common space prefix, drop it from all lines
    if space_prefix_len:
      for index in xrange(len(text_lines)):
        if text_lines[index]:
          text_lines[index] = text_lines[index][space_prefix_len:]
    return '\n'.join(text_first_line + text_lines)
  return ''


def TextWrap(text, length=None, indent='', firstline_indent=None, tabs='    '):
  """Wraps a given text to a maximum line length and returns it.

  We turn lines that only contain whitespace into empty lines.  We keep
  new lines and tabs (e.g., we do not treat tabs as spaces).

  Args:
    text:             text to wrap
    length:           maximum length of a line, includes indentation
                      if this is None then use GetHelpWidth()
    indent:           indent for all but first line
    firstline_indent: indent for first line; if None, fall back to indent
    tabs:             replacement for tabs

  Returns:
    wrapped text

  Raises:
    FlagsError: if indent not shorter than length
    FlagsError: if firstline_indent not shorter than length
  """
  # Get defaults where callee used None
  if length is None:
    length = GetHelpWidth()
  if indent is None:
    indent = ''
  if len(indent) >= length:
    raise FlagsError('Indent must be shorter than length')
  # In line we will be holding the current line which is to be started
  # with indent (or firstline_indent if available) and then appended
  # with words.
  if firstline_indent is None:
    firstline_indent = ''
    line = indent
  else:
    line = firstline_indent
    if len(firstline_indent) >= length:
      raise FlagsError('First line indent must be shorter than length')

  # If the callee does not care about tabs we simply convert them to
  # spaces If callee wanted tabs to be single space then we do that
  # already here.
  if not tabs or tabs == ' ':
    text = text.replace('\t', ' ')
  else:
    tabs_are_whitespace = not tabs.strip()

  line_regex = re.compile('([ ]*)(\t*)([^ \t]+)', re.MULTILINE)

  # Split the text into lines and the lines with the regex above. The
  # resulting lines are collected in result[]. For each split we get the
  # spaces, the tabs and the next non white space (e.g. next word).
  result = []
  for text_line in text.splitlines():
    # Store result length so we can find out whether processing the next
    # line gave any new content
    old_result_len = len(result)
    # Process next line with line_regex. For optimization we do an rstrip().
    # - process tabs (changes either line or word, see below)
    # - process word (first try to squeeze on line, then wrap or force wrap)
    # Spaces found on the line are ignored, they get added while wrapping as
    # needed.
    for spaces, current_tabs, word in line_regex.findall(text_line.rstrip()):
      # If tabs weren't converted to spaces, handle them now
      if current_tabs:
        # If the last thing we added was a space anyway then drop
        # it. But let's not get rid of the indentation.
        if (((result and line != indent) or
             (not result and line != firstline_indent)) and line[-1] == ' '):
          line = line[:-1]
        # Add the tabs, if that means adding whitespace, just add it at
        # the line, the rstrip() code while shorten the line down if
        # necessary
        if tabs_are_whitespace:
          line += tabs * len(current_tabs)
        else:
          # if not all tab replacement is whitespace we prepend it to the word
          word = tabs * len(current_tabs) + word
      # Handle the case where word cannot be squeezed onto current last line
      if len(line) + len(word) > length and len(indent) + len(word) <= length:
        result.append(line.rstrip())
        line = indent + word
        word = ''
        # No space left on line or can we append a space?
        if len(line) + 1 >= length:
          result.append(line.rstrip())
          line = indent
        else:
          line += ' '
      # Add word and shorten it up to allowed line length. Restart next
      # line with indent and repeat, or add a space if we're done (word
      # finished) This deals with words that cannot fit on one line
      # (e.g. indent + word longer than allowed line length).
      while len(line) + len(word) >= length:
        line += word
        result.append(line[:length])
        word = line[length:]
        line = indent
      # Default case, simply append the word and a space
      if word:
        line += word + ' '
    # End of input line. If we have content we finish the line. If the
    # current line is just the indent but we had content in during this
    # original line then we need to add an empty line.
    if (result and line != indent) or (not result and line != firstline_indent):
      result.append(line.rstrip())
    elif len(result) == old_result_len:
      result.append('')
    line = indent

  return '\n'.join(result)


def DocToHelp(doc):
  """Takes a __doc__ string and reformats it as help."""

  # Get rid of starting and ending white space. Using lstrip() or even
  # strip() could drop more than maximum of first line and right space
  # of last line.
  doc = doc.strip()

  # Get rid of all empty lines
  whitespace_only_line = re.compile('^[ \t]+$', re.M)
  doc = whitespace_only_line.sub('', doc)

  # Cut out common space at line beginnings
  doc = CutCommonSpacePrefix(doc)

  # Just like this module's comment, comments tend to be aligned somehow.
  # In other words they all start with the same amount of white space
  # 1) keep double new lines
  # 2) keep ws after new lines if not empty line
  # 3) all other new lines shall be changed to a space
  # Solution: Match new lines between non white space and replace with space.
  doc = re.sub('(?<=\S)\n(?=\S)', ' ', doc, re.M)

  return doc


def _GetModuleObjectAndName(globals_dict):
  """Returns the module that defines a global environment, and its name.

  Args:
    globals_dict: A dictionary that should correspond to an environment
      providing the values of the globals.

  Returns:
    A pair consisting of (1) module object and (2) module name (a
    string).  Returns (None, None) if the module could not be
    identified.
  """
  # The use of .items() (instead of .iteritems()) is NOT a mistake: if
  # a parallel thread imports a module while we iterate over
  # .iteritems() (not nice, but possible), we get a RuntimeError ...
  # Hence, we use the slightly slower but safer .items().
  for name, module in sys.modules.items():
    if getattr(module, '__dict__', None) is globals_dict:
      if name == '__main__':
        # Pick a more informative name for the main module.
        name = sys.argv[0]
      return (module, name)
  return (None, None)


def _GetMainModule():
  """Returns: string, name of the module from which execution started."""
  # First, try to use the same logic used by _GetCallingModuleObjectAndName(),
  # i.e., call _GetModuleObjectAndName().  For that we first need to
  # find the dictionary that the main module uses to store the
  # globals.
  #
  # That's (normally) the same dictionary object that the deepest
  # (oldest) stack frame is using for globals.
  deepest_frame = sys._getframe(0)
  while deepest_frame.f_back is not None:
    deepest_frame = deepest_frame.f_back
  globals_for_main_module = deepest_frame.f_globals
  main_module_name = _GetModuleObjectAndName(globals_for_main_module)[1]
  # The above strategy fails in some cases (e.g., tools that compute
  # code coverage by redefining, among other things, the main module).
  # If so, just use sys.argv[0].  We can probably always do this, but
  # it's safest to try to use the same logic as _GetCallingModuleObjectAndName()
  if main_module_name is None:
    main_module_name = sys.argv[0]
  return main_module_name


class FlagValues:
  """Registry of 'Flag' objects.

  A 'FlagValues' can then scan command line arguments, passing flag
  arguments through to the 'Flag' objects that it owns.  It also
  provides easy access to the flag values.  Typically only one
  'FlagValues' object is needed by an application: gflags.FLAGS

  This class is heavily overloaded:

  'Flag' objects are registered via __setitem__:
       FLAGS['longname'] = x   # register a new flag

  The .value attribute of the registered 'Flag' objects can be accessed
  as attributes of this 'FlagValues' object, through __getattr__.  Both
  the long and short name of the original 'Flag' objects can be used to
  access its value:
       FLAGS.longname          # parsed flag value
       FLAGS.x                 # parsed flag value (short name)

  Command line arguments are scanned and passed to the registered 'Flag'
  objects through the __call__ method.  Unparsed arguments, including
  argv[0] (e.g. the program name) are returned.
       argv = FLAGS(sys.argv)  # scan command line arguments

  The original registered Flag objects can be retrieved through the use
  of the dictionary-like operator, __getitem__:
       x = FLAGS['longname']   # access the registered Flag object

  The str() operator of a 'FlagValues' object provides help for all of
  the registered 'Flag' objects.
  """

  def __init__(self):
    # Since everything in this class is so heavily overloaded, the only
    # way of defining and using fields is to access __dict__ directly.

    # Dictionary: flag name (string) -> Flag object.
    self.__dict__['__flags'] = {}
    # Dictionary: module name (string) -> list of Flag objects that are defined
    # by that module.
    self.__dict__['__flags_by_module'] = {}
    # Dictionary: module id (int) -> list of Flag objects that are defined by
    # that module.
    self.__dict__['__flags_by_module_id'] = {}
    # Dictionary: module name (string) -> list of Flag objects that are
    # key for that module.
    self.__dict__['__key_flags_by_module'] = {}

    # Set if we should use new style gnu_getopt rather than getopt when parsing
    # the args.  Only possible with Python 2.3+
    self.UseGnuGetOpt(False)

  def UseGnuGetOpt(self, use_gnu_getopt=True):
    """Use GNU-style scanning. Allows mixing of flag and non-flag arguments.

    See http://docs.python.org/library/getopt.html#getopt.gnu_getopt

    Args:
      use_gnu_getopt: wether or not to use GNU style scanning.
    """
    self.__dict__['__use_gnu_getopt'] = use_gnu_getopt

  def IsGnuGetOpt(self):
    return self.__dict__['__use_gnu_getopt']

  def FlagDict(self):
    return self.__dict__['__flags']

  def FlagsByModuleDict(self):
    """Returns the dictionary of module_name -> list of defined flags.

    Returns:
      A dictionary.  Its keys are module names (strings).  Its values
      are lists of Flag objects.
    """
    return self.__dict__['__flags_by_module']

  def FlagsByModuleIdDict(self):
    """Returns the dictionary of module_id -> list of defined flags.

    Returns:
      A dictionary.  Its keys are module IDs (ints).  Its values
      are lists of Flag objects.
    """
    return self.__dict__['__flags_by_module_id']

  def KeyFlagsByModuleDict(self):
    """Returns the dictionary of module_name -> list of key flags.

    Returns:
      A dictionary.  Its keys are module names (strings).  Its values
      are lists of Flag objects.
    """
    return self.__dict__['__key_flags_by_module']

  def _RegisterFlagByModule(self, module_name, flag):
    """Records the module that defines a specific flag.

    We keep track of which flag is defined by which module so that we
    can later sort the flags by module.

    Args:
      module_name: A string, the name of a Python module.
      flag: A Flag object, a flag that is key to the module.
    """
    flags_by_module = self.FlagsByModuleDict()
    flags_by_module.setdefault(module_name, []).append(flag)

  def _RegisterFlagByModuleId(self, module_id, flag):
    """Records the module that defines a specific flag.

    Args:
      module_id: An int, the ID of the Python module.
      flag: A Flag object, a flag that is key to the module.
    """
    flags_by_module_id = self.FlagsByModuleIdDict()
    flags_by_module_id.setdefault(module_id, []).append(flag)

  def _RegisterKeyFlagForModule(self, module_name, flag):
    """Specifies that a flag is a key flag for a module.

    Args:
      module_name: A string, the name of a Python module.
      flag: A Flag object, a flag that is key to the module.
    """
    key_flags_by_module = self.KeyFlagsByModuleDict()
    # The list of key flags for the module named module_name.
    key_flags = key_flags_by_module.setdefault(module_name, [])
    # Add flag, but avoid duplicates.
    if flag not in key_flags:
      key_flags.append(flag)

  def _GetFlagsDefinedByModule(self, module):
    """Returns the list of flags defined by a module.

    Args:
      module: A module object or a module name (a string).

    Returns:
      A new list of Flag objects.  Caller may update this list as he
      wishes: none of those changes will affect the internals of this
      FlagValue object.
    """
    if not isinstance(module, str):
      module = module.__name__

    return list(self.FlagsByModuleDict().get(module, []))

  def _GetKeyFlagsForModule(self, module):
    """Returns the list of key flags for a module.

    Args:
      module: A module object or a module name (a string)

    Returns:
      A new list of Flag objects.  Caller may update this list as he
      wishes: none of those changes will affect the internals of this
      FlagValue object.
    """
    if not isinstance(module, str):
      module = module.__name__

    # Any flag is a key flag for the module that defined it.  NOTE:
    # key_flags is a fresh list: we can update it without affecting the
    # internals of this FlagValues object.
    key_flags = self._GetFlagsDefinedByModule(module)

    # Take into account flags explicitly declared as key for a module.
    for flag in self.KeyFlagsByModuleDict().get(module, []):
      if flag not in key_flags:
        key_flags.append(flag)
    return key_flags

  def FindModuleDefiningFlag(self, flagname, default=None):
    """Return the name of the module defining this flag, or default.

    Args:
      flagname: Name of the flag to lookup.
      default: Value to return if flagname is not defined. Defaults
          to None.

    Returns:
      The name of the module which registered the flag with this name.
      If no such module exists (i.e. no flag with this name exists),
      we return default.
    """
    for module, flags in self.FlagsByModuleDict().iteritems():
      for flag in flags:
        if flag.name == flagname or flag.short_name == flagname:
          return module
    return default

  def FindModuleIdDefiningFlag(self, flagname, default=None):
    """Return the ID of the module defining this flag, or default.

    Args:
      flagname: Name of the flag to lookup.
      default: Value to return if flagname is not defined. Defaults
          to None.

    Returns:
      The ID of the module which registered the flag with this name.
      If no such module exists (i.e. no flag with this name exists),
      we return default.
    """
    for module_id, flags in self.FlagsByModuleIdDict().iteritems():
      for flag in flags:
        if flag.name == flagname or flag.short_name == flagname:
          return module_id
    return default

  def AppendFlagValues(self, flag_values):
    """Appends flags registered in another FlagValues instance.

    Args:
      flag_values: registry to copy from
    """
    for flag_name, flag in flag_values.FlagDict().iteritems():
      # Each flags with shortname appears here twice (once under its
      # normal name, and again with its short name).  To prevent
      # problems (DuplicateFlagError) with double flag registration, we
      # perform a check to make sure that the entry we're looking at is
      # for its normal name.
      if flag_name == flag.name:
        try:
          self[flag_name] = flag
        except DuplicateFlagError:
          raise DuplicateFlagError(flag_name, self,
                                   other_flag_values=flag_values)

  def RemoveFlagValues(self, flag_values):
    """Remove flags that were previously appended from another FlagValues.

    Args:
      flag_values: registry containing flags to remove.
    """
    for flag_name in flag_values.FlagDict():
      self.__delattr__(flag_name)

  def __setitem__(self, name, flag):
    """Registers a new flag variable."""
    fl = self.FlagDict()
    if not isinstance(flag, Flag):
      raise IllegalFlagValue(flag)
    if not isinstance(name, type("")):
      raise FlagsError("Flag name must be a string")
    if len(name) == 0:
      raise FlagsError("Flag name cannot be empty")
    # If running under pychecker, duplicate keys are likely to be
    # defined.  Disable check for duplicate keys when pycheck'ing.
    if (name in fl and not flag.allow_override and
        not fl[name].allow_override and not _RUNNING_PYCHECKER):
      module, module_name = _GetCallingModuleObjectAndName()
      if (self.FindModuleDefiningFlag(name) == module_name and
          id(module) != self.FindModuleIdDefiningFlag(name)):
        # If the flag has already been defined by a module with the same name,
        # but a different ID, we can stop here because it indicates that the
        # module is simply being imported a subsequent time.
        return
      raise DuplicateFlagError(name, self)
    short_name = flag.short_name
    if short_name is not None:
      if (short_name in fl and not flag.allow_override and
          not fl[short_name].allow_override and not _RUNNING_PYCHECKER):
        raise DuplicateFlagError(short_name, self)
      fl[short_name] = flag
    fl[name] = flag
    global _exported_flags
    _exported_flags[name] = flag

  def __getitem__(self, name):
    """Retrieves the Flag object for the flag --name."""
    return self.FlagDict()[name]

  def __getattr__(self, name):
    """Retrieves the 'value' attribute of the flag --name."""
    fl = self.FlagDict()
    if name not in fl:
      raise AttributeError(name)
    return fl[name].value

  def __setattr__(self, name, value):
    """Sets the 'value' attribute of the flag --name."""
    fl = self.FlagDict()
    fl[name].value = value
    self._AssertValidators(fl[name].validators)
    return value

  def _AssertAllValidators(self):
    all_validators = set()
    for flag in self.FlagDict().itervalues():
      for validator in flag.validators:
        all_validators.add(validator)
    self._AssertValidators(all_validators)

  def _AssertValidators(self, validators):
    """Assert if all validators in the list are satisfied.

    Asserts validators in the order they were created.
    Args:
      validators: Iterable(gflags_validators.Validator), validators to be
        verified
    Raises:
      AttributeError: if validators work with a non-existing flag.
      IllegalFlagValue: if validation fails for at least one validator
    """
    for validator in sorted(
        validators, key=lambda validator: validator.insertion_index):
      try:
        validator.Verify(self)
      except gflags_validators.Error, e:
        message = validator.PrintFlagsWithValues(self)
        raise IllegalFlagValue('%s: %s' % (message, str(e)))

  def _FlagIsRegistered(self, flag_obj):
    """Checks whether a Flag object is registered under some name.

    Note: this is non trivial: in addition to its normal name, a flag
    may have a short name too.  In self.FlagDict(), both the normal and
    the short name are mapped to the same flag object.  E.g., calling
    only "del FLAGS.short_name" is not unregistering the corresponding
    Flag object (it is still registered under the longer name).

    Args:
      flag_obj: A Flag object.

    Returns:
      A boolean: True iff flag_obj is registered under some name.
    """
    flag_dict = self.FlagDict()
    # Check whether flag_obj is registered under its long name.
    name = flag_obj.name
    if flag_dict.get(name, None) == flag_obj:
      return True
    # Check whether flag_obj is registered under its short name.
    short_name = flag_obj.short_name
    if (short_name is not None and
        flag_dict.get(short_name, None) == flag_obj):
      return True
    # The flag cannot be registered under any other name, so we do not
    # need to do a full search through the values of self.FlagDict().
    return False

  def __delattr__(self, flag_name):
    """Deletes a previously-defined flag from a flag object.

    This method makes sure we can delete a flag by using

      del flag_values_object.<flag_name>

    E.g.,

      gflags.DEFINE_integer('foo', 1, 'Integer flag.')
      del gflags.FLAGS.foo

    Args:
      flag_name: A string, the name of the flag to be deleted.

    Raises:
      AttributeError: When there is no registered flag named flag_name.
    """
    fl = self.FlagDict()
    if flag_name not in fl:
      raise AttributeError(flag_name)

    flag_obj = fl[flag_name]
    del fl[flag_name]

    if not self._FlagIsRegistered(flag_obj):
      # If the Flag object indicated by flag_name is no longer
      # registered (please see the docstring of _FlagIsRegistered), then
      # we delete the occurrences of the flag object in all our internal
      # dictionaries.
      self.__RemoveFlagFromDictByModule(self.FlagsByModuleDict(), flag_obj)
      self.__RemoveFlagFromDictByModule(self.FlagsByModuleIdDict(), flag_obj)
      self.__RemoveFlagFromDictByModule(self.KeyFlagsByModuleDict(), flag_obj)

  def __RemoveFlagFromDictByModule(self, flags_by_module_dict, flag_obj):
    """Removes a flag object from a module -> list of flags dictionary.

    Args:
      flags_by_module_dict: A dictionary that maps module names to lists of
        flags.
      flag_obj: A flag object.
    """
    for unused_module, flags_in_module in flags_by_module_dict.iteritems():
      # while (as opposed to if) takes care of multiple occurrences of a
      # flag in the list for the same module.
      while flag_obj in flags_in_module:
        flags_in_module.remove(flag_obj)

  def SetDefault(self, name, value):
    """Changes the default value of the named flag object."""
    fl = self.FlagDict()
    if name not in fl:
      raise AttributeError(name)
    fl[name].SetDefault(value)
    self._AssertValidators(fl[name].validators)

  def __contains__(self, name):
    """Returns True if name is a value (flag) in the dict."""
    return name in self.FlagDict()

  has_key = __contains__  # a synonym for __contains__()

  def __iter__(self):
    return iter(self.FlagDict())

  def __call__(self, argv):
    """Parses flags from argv; stores parsed flags into this FlagValues object.

    All unparsed arguments are returned.  Flags are parsed using the GNU
    Program Argument Syntax Conventions, using getopt:

    http://www.gnu.org/software/libc/manual/html_mono/libc.html#Getopt

    Args:
       argv: argument list. Can be of any type that may be converted to a list.

    Returns:
       The list of arguments not parsed as options, including argv[0]

    Raises:
       FlagsError: on any parsing error
    """
    # Support any sequence type that can be converted to a list
    argv = list(argv)

    shortopts = ""
    longopts = []

    fl = self.FlagDict()

    # This pre parses the argv list for --flagfile=<> options.
    argv = argv[:1] + self.ReadFlagsFromFiles(argv[1:], force_gnu=False)

    # Correct the argv to support the google style of passing boolean
    # parameters.  Boolean parameters may be passed by using --mybool,
    # --nomybool, --mybool=(true|false|1|0).  getopt does not support
    # having options that may or may not have a parameter.  We replace
    # instances of the short form --mybool and --nomybool with their
    # full forms: --mybool=(true|false).
    original_argv = list(argv)  # list() makes a copy
    shortest_matches = None
    for name, flag in fl.items():
      if not flag.boolean:
        continue
      if shortest_matches is None:
        # Determine the smallest allowable prefix for all flag names
        shortest_matches = self.ShortestUniquePrefixes(fl)
      no_name = 'no' + name
      prefix = shortest_matches[name]
      no_prefix = shortest_matches[no_name]

      # Replace all occurrences of this boolean with extended forms
      for arg_idx in range(1, len(argv)):
        arg = argv[arg_idx]
        if arg.find('=') >= 0: continue
        if arg.startswith('--'+prefix) and ('--'+name).startswith(arg):
          argv[arg_idx] = ('--%s=true' % name)
        elif arg.startswith('--'+no_prefix) and ('--'+no_name).startswith(arg):
          argv[arg_idx] = ('--%s=false' % name)

    # Loop over all of the flags, building up the lists of short options
    # and long options that will be passed to getopt.  Short options are
    # specified as a string of letters, each letter followed by a colon
    # if it takes an argument.  Long options are stored in an array of
    # strings.  Each string ends with an '=' if it takes an argument.
    for name, flag in fl.items():
      longopts.append(name + "=")
      if len(name) == 1:  # one-letter option: allow short flag type also
        shortopts += name
        if not flag.boolean:
          shortopts += ":"

    longopts.append('undefok=')
    undefok_flags = []

    # In case --undefok is specified, loop to pick up unrecognized
    # options one by one.
    unrecognized_opts = []
    args = argv[1:]
    while True:
      try:
        if self.__dict__['__use_gnu_getopt']:
          optlist, unparsed_args = getopt.gnu_getopt(args, shortopts, longopts)
        else:
          optlist, unparsed_args = getopt.getopt(args, shortopts, longopts)
        break
      except getopt.GetoptError, e:
        if not e.opt or e.opt in fl:
          # Not an unrecognized option, re-raise the exception as a FlagsError
          raise FlagsError(e)
        # Remove offender from args and try again
        for arg_index in range(len(args)):
          if ((args[arg_index] == '--' + e.opt) or
              (args[arg_index] == '-' + e.opt) or
              (args[arg_index].startswith('--' + e.opt + '='))):
            unrecognized_opts.append((e.opt, args[arg_index]))
            args = args[0:arg_index] + args[arg_index+1:]
            break
        else:
          # We should have found the option, so we don't expect to get
          # here.  We could assert, but raising the original exception
          # might work better.
          raise FlagsError(e)

    for name, arg in optlist:
      if name == '--undefok':
        flag_names = arg.split(',')
        undefok_flags.extend(flag_names)
        # For boolean flags, if --undefok=boolflag is specified, then we should
        # also accept --noboolflag, in addition to --boolflag.
        # Since we don't know the type of the undefok'd flag, this will affect
        # non-boolean flags as well.
        # NOTE: You shouldn't use --undefok=noboolflag, because then we will
        # accept --nonoboolflag here.  We are choosing not to do the conversion
        # from noboolflag -> boolflag because of the ambiguity that flag names
        # can start with 'no'.
        undefok_flags.extend('no' + name for name in flag_names)
        continue
      if name.startswith('--'):
        # long option
        name = name[2:]
        short_option = 0
      else:
        # short option
        name = name[1:]
        short_option = 1
      if name in fl:
        flag = fl[name]
        if flag.boolean and short_option: arg = 1
        flag.Parse(arg)

    # If there were unrecognized options, raise an exception unless
    # the options were named via --undefok.
    for opt, value in unrecognized_opts:
      if opt not in undefok_flags:
        raise UnrecognizedFlagError(opt, value)

    if unparsed_args:
      if self.__dict__['__use_gnu_getopt']:
        # if using gnu_getopt just return the program name + remainder of argv.
        ret_val = argv[:1] + unparsed_args
      else:
        # unparsed_args becomes the first non-flag detected by getopt to
        # the end of argv.  Because argv may have been modified above,
        # return original_argv for this region.
        ret_val = argv[:1] + original_argv[-len(unparsed_args):]
    else:
      ret_val = argv[:1]

    self._AssertAllValidators()
    return ret_val

  def Reset(self):
    """Resets the values to the point before FLAGS(argv) was called."""
    for f in self.FlagDict().values():
      f.Unparse()

  def RegisteredFlags(self):
    """Returns: a list of the names and short names of all registered flags."""
    return list(self.FlagDict())

  def FlagValuesDict(self):
    """Returns: a dictionary that maps flag names to flag values."""
    flag_values = {}

    for flag_name in self.RegisteredFlags():
      flag = self.FlagDict()[flag_name]
      flag_values[flag_name] = flag.value

    return flag_values

  def __str__(self):
    """Generates a help string for all known flags."""
    return self.GetHelp()

  def GetHelp(self, prefix=''):
    """Generates a help string for all known flags."""
    helplist = []

    flags_by_module = self.FlagsByModuleDict()
    if flags_by_module:

      modules = sorted(flags_by_module)

      # Print the help for the main module first, if possible.
      main_module = _GetMainModule()
      if main_module in modules:
        modules.remove(main_module)
        modules = [main_module] + modules

      for module in modules:
        self.__RenderOurModuleFlags(module, helplist)

      self.__RenderModuleFlags('gflags',
                               _SPECIAL_FLAGS.FlagDict().values(),
                               helplist)

    else:
      # Just print one long list of flags.
      self.__RenderFlagList(
          self.FlagDict().values() + _SPECIAL_FLAGS.FlagDict().values(),
          helplist, prefix)

    return '\n'.join(helplist)

  def __RenderModuleFlags(self, module, flags, output_lines, prefix=""):
    """Generates a help string for a given module."""
    if not isinstance(module, str):
      module = module.__name__
    output_lines.append('\n%s%s:' % (prefix, module))
    self.__RenderFlagList(flags, output_lines, prefix + "  ")

  def __RenderOurModuleFlags(self, module, output_lines, prefix=""):
    """Generates a help string for a given module."""
    flags = self._GetFlagsDefinedByModule(module)
    if flags:
      self.__RenderModuleFlags(module, flags, output_lines, prefix)

  def __RenderOurModuleKeyFlags(self, module, output_lines, prefix=""):
    """Generates a help string for the key flags of a given module.

    Args:
      module: A module object or a module name (a string).
      output_lines: A list of strings.  The generated help message
        lines will be appended to this list.
      prefix: A string that is prepended to each generated help line.
    """
    key_flags = self._GetKeyFlagsForModule(module)
    if key_flags:
      self.__RenderModuleFlags(module, key_flags, output_lines, prefix)

  def ModuleHelp(self, module):
    """Describe the key flags of a module.

    Args:
      module: A module object or a module name (a string).

    Returns:
      string describing the key flags of a module.
    """
    helplist = []
    self.__RenderOurModuleKeyFlags(module, helplist)
    return '\n'.join(helplist)

  def MainModuleHelp(self):
    """Describe the key flags of the main module.

    Returns:
      string describing the key flags of a module.
    """
    return self.ModuleHelp(_GetMainModule())

  def __RenderFlagList(self, flaglist, output_lines, prefix="  "):
    fl = self.FlagDict()
    special_fl = _SPECIAL_FLAGS.FlagDict()
    flaglist = [(flag.name, flag) for flag in flaglist]
    flaglist.sort()
    flagset = {}
    for (name, flag) in flaglist:
      # It's possible this flag got deleted or overridden since being
      # registered in the per-module flaglist.  Check now against the
      # canonical source of current flag information, the FlagDict.
      if fl.get(name, None) != flag and special_fl.get(name, None) != flag:
        # a different flag is using this name now
        continue
      # only print help once
      if flag in flagset: continue
      flagset[flag] = 1
      flaghelp = ""
      if flag.short_name: flaghelp += "-%s," % flag.short_name
      if flag.boolean:
        flaghelp += "--[no]%s" % flag.name + ":"
      else:
        flaghelp += "--%s" % flag.name + ":"
      flaghelp += "  "
      if flag.help:
        flaghelp += flag.help
      flaghelp = TextWrap(flaghelp, indent=prefix+"  ",
                          firstline_indent=prefix)
      if flag.default_as_str:
        flaghelp += "\n"
        flaghelp += TextWrap("(default: %s)" % flag.default_as_str,
                             indent=prefix+"  ")
      if flag.parser.syntactic_help:
        flaghelp += "\n"
        flaghelp += TextWrap("(%s)" % flag.parser.syntactic_help,
                             indent=prefix+"  ")
      output_lines.append(flaghelp)

  def get(self, name, default):
    """Returns the value of a flag (if not None) or a default value.

    Args:
      name: A string, the name of a flag.
      default: Default value to use if the flag value is None.
    """

    value = self.__getattr__(name)
    if value is not None:  # Can't do if not value, b/c value might be '0' or ""
      return value
    else:
      return default

  def ShortestUniquePrefixes(self, fl):
    """Returns: dictionary; maps flag names to their shortest unique prefix."""
    # Sort the list of flag names
    sorted_flags = []
    for name, flag in fl.items():
      sorted_flags.append(name)
      if flag.boolean:
        sorted_flags.append('no%s' % name)
    sorted_flags.sort()

    # For each name in the sorted list, determine the shortest unique
    # prefix by comparing itself to the next name and to the previous
    # name (the latter check uses cached info from the previous loop).
    shortest_matches = {}
    prev_idx = 0
    for flag_idx in range(len(sorted_flags)):
      curr = sorted_flags[flag_idx]
      if flag_idx == (len(sorted_flags) - 1):
        next = None
      else:
        next = sorted_flags[flag_idx+1]
        next_len = len(next)
      for curr_idx in range(len(curr)):
        if (next is None
            or curr_idx >= next_len
            or curr[curr_idx] != next[curr_idx]):
          # curr longer than next or no more chars in common
          shortest_matches[curr] = curr[:max(prev_idx, curr_idx) + 1]
          prev_idx = curr_idx
          break
      else:
        # curr shorter than (or equal to) next
        shortest_matches[curr] = curr
        prev_idx = curr_idx + 1  # next will need at least one more char
    return shortest_matches

  def __IsFlagFileDirective(self, flag_string):
    """Checks whether flag_string contain a --flagfile=<foo> directive."""
    if isinstance(flag_string, type("")):
      if flag_string.startswith('--flagfile='):
        return 1
      elif flag_string == '--flagfile':
        return 1
      elif flag_string.startswith('-flagfile='):
        return 1
      elif flag_string == '-flagfile':
        return 1
      else:
        return 0
    return 0

  def ExtractFilename(self, flagfile_str):
    """Returns filename from a flagfile_str of form -[-]flagfile=filename.

    The cases of --flagfile foo and -flagfile foo shouldn't be hitting
    this function, as they are dealt with in the level above this
    function.
    """
    if flagfile_str.startswith('--flagfile='):
      return os.path.expanduser((flagfile_str[(len('--flagfile=')):]).strip())
    elif flagfile_str.startswith('-flagfile='):
      return os.path.expanduser((flagfile_str[(len('-flagfile=')):]).strip())
    else:
      raise FlagsError('Hit illegal --flagfile type: %s' % flagfile_str)

  def __GetFlagFileLines(self, filename, parsed_file_list):
    """Returns the useful (!=comments, etc) lines from a file with flags.

    Args:
      filename: A string, the name of the flag file.
      parsed_file_list: A list of the names of the files we have
        already read.  MUTATED BY THIS FUNCTION.

    Returns:
      List of strings. See the note below.

    NOTE(springer): This function checks for a nested --flagfile=<foo>
    tag and handles the lower file recursively. It returns a list of
    all the lines that _could_ contain command flags. This is
    EVERYTHING except whitespace lines and comments (lines starting
    with '#' or '//').
    """
    line_list = []  # All line from flagfile.
    flag_line_list = []  # Subset of lines w/o comments, blanks, flagfile= tags.
    try:
      file_obj = open(filename, 'r')
    except IOError, e_msg:
      raise CantOpenFlagFileError('ERROR:: Unable to open flagfile: %s' % e_msg)

    line_list = file_obj.readlines()
    file_obj.close()
    parsed_file_list.append(filename)

    # This is where we check each line in the file we just read.
    for line in line_list:
      if line.isspace():
        pass
      # Checks for comment (a line that starts with '#').
      elif line.startswith('#') or line.startswith('//'):
        pass
      # Checks for a nested "--flagfile=<bar>" flag in the current file.
      # If we find one, recursively parse down into that file.
      elif self.__IsFlagFileDirective(line):
        sub_filename = self.ExtractFilename(line)
        # We do a little safety check for reparsing a file we've already done.
        if not sub_filename in parsed_file_list:
          included_flags = self.__GetFlagFileLines(sub_filename,
                                                   parsed_file_list)
          flag_line_list.extend(included_flags)
        else:  # Case of hitting a circularly included file.
          sys.stderr.write('Warning: Hit circular flagfile dependency: %s\n' %
                           (sub_filename,))
      else:
        # Any line that's not a comment or a nested flagfile should get
        # copied into 2nd position.  This leaves earlier arguments
        # further back in the list, thus giving them higher priority.
        flag_line_list.append(line.strip())
    return flag_line_list

  def ReadFlagsFromFiles(self, argv, force_gnu=True):
    """Processes command line args, but also allow args to be read from file.

    Args:
      argv: A list of strings, usually sys.argv[1:], which may contain one or
        more flagfile directives of the form --flagfile="./filename".
        Note that the name of the program (sys.argv[0]) should be omitted.
      force_gnu: If False, --flagfile parsing obeys normal flag semantics.
        If True, --flagfile parsing instead follows gnu_getopt semantics.
        *** WARNING *** force_gnu=False may become the future default!

    Returns:

      A new list which has the original list combined with what we read
      from any flagfile(s).

    References: Global gflags.FLAG class instance.

    This function should be called before the normal FLAGS(argv) call.
    This function scans the input list for a flag that looks like:
    --flagfile=<somefile>. Then it opens <somefile>, reads all valid key
    and value pairs and inserts them into the input list between the
    first item of the list and any subsequent items in the list.

    Note that your application's flags are still defined the usual way
    using gflags DEFINE_flag() type functions.

    Notes (assuming we're getting a commandline of some sort as our input):
    --> Flags from the command line argv _should_ always take precedence!
    --> A further "--flagfile=<otherfile.cfg>" CAN be nested in a flagfile.
        It will be processed after the parent flag file is done.
    --> For duplicate flags, first one we hit should "win".
    --> In a flagfile, a line beginning with # or // is a comment.
    --> Entirely blank lines _should_ be ignored.
    """
    parsed_file_list = []
    rest_of_args = argv
    new_argv = []
    while rest_of_args:
      current_arg = rest_of_args[0]
      rest_of_args = rest_of_args[1:]
      if self.__IsFlagFileDirective(current_arg):
        # This handles the case of -(-)flagfile foo.  In this case the
        # next arg really is part of this one.
        if current_arg == '--flagfile' or current_arg == '-flagfile':
          if not rest_of_args:
            raise IllegalFlagValue('--flagfile with no argument')
          flag_filename = os.path.expanduser(rest_of_args[0])
          rest_of_args = rest_of_args[1:]
        else:
          # This handles the case of (-)-flagfile=foo.
          flag_filename = self.ExtractFilename(current_arg)
        new_argv.extend(
            self.__GetFlagFileLines(flag_filename, parsed_file_list))
      else:
        new_argv.append(current_arg)
        # Stop parsing after '--', like getopt and gnu_getopt.
        if current_arg == '--':
          break
        # Stop parsing after a non-flag, like getopt.
        if not current_arg.startswith('-'):
          if not force_gnu and not self.__dict__['__use_gnu_getopt']:
            break

    if rest_of_args:
      new_argv.extend(rest_of_args)

    return new_argv

  def FlagsIntoString(self):
    """Returns a string with the flags assignments from this FlagValues object.

    This function ignores flags whose value is None.  Each flag
    assignment is separated by a newline.

    NOTE: MUST mirror the behavior of the C++ CommandlineFlagsIntoString
    from http://code.google.com/p/google-gflags
    """
    s = ''
    for flag in self.FlagDict().values():
      if flag.value is not None:
        s += flag.Serialize() + '\n'
    return s

  def AppendFlagsIntoFile(self, filename):
    """Appends all flags assignments from this FlagInfo object to a file.

    Output will be in the format of a flagfile.

    NOTE: MUST mirror the behavior of the C++ AppendFlagsIntoFile
    from http://code.google.com/p/google-gflags
    """
    out_file = open(filename, 'a')
    out_file.write(self.FlagsIntoString())
    out_file.close()

  def WriteHelpInXMLFormat(self, outfile=None):
    """Outputs flag documentation in XML format.

    NOTE: We use element names that are consistent with those used by
    the C++ command-line flag library, from
    http://code.google.com/p/google-gflags
    We also use a few new elements (e.g., <key>), but we do not
    interfere / overlap with existing XML elements used by the C++
    library.  Please maintain this consistency.

    Args:
      outfile: File object we write to.  Default None means sys.stdout.
    """
    outfile = outfile or sys.stdout

    outfile.write('<?xml version=\"1.0\"?>\n')
    outfile.write('<AllFlags>\n')
    indent = '  '
    _WriteSimpleXMLElement(outfile, 'program', os.path.basename(sys.argv[0]),
                           indent)

    usage_doc = sys.modules['__main__'].__doc__
    if not usage_doc:
      usage_doc = '\nUSAGE: %s [flags]\n' % sys.argv[0]
    else:
      usage_doc = usage_doc.replace('%s', sys.argv[0])
    _WriteSimpleXMLElement(outfile, 'usage', usage_doc, indent)

    # Get list of key flags for the main module.
    key_flags = self._GetKeyFlagsForModule(_GetMainModule())

    # Sort flags by declaring module name and next by flag name.
    flags_by_module = self.FlagsByModuleDict()
    all_module_names = list(flags_by_module.keys())
    all_module_names.sort()
    for module_name in all_module_names:
      flag_list = [(f.name, f) for f in flags_by_module[module_name]]
      flag_list.sort()
      for unused_flag_name, flag in flag_list:
        is_key = flag in key_flags
        flag.WriteInfoInXMLFormat(outfile, module_name,
                                  is_key=is_key, indent=indent)

    outfile.write('</AllFlags>\n')
    outfile.flush()

  def AddValidator(self, validator):
    """Register new flags validator to be checked.

    Args:
      validator: gflags_validators.Validator
    Raises:
      AttributeError: if validators work with a non-existing flag.
    """
    for flag_name in validator.GetFlagsNames():
      flag = self.FlagDict()[flag_name]
      flag.validators.append(validator)

# end of FlagValues definition


# The global FlagValues instance
FLAGS = FlagValues()


def _StrOrUnicode(value):
  """Converts value to a python string or, if necessary, unicode-string."""
  try:
    return str(value)
  except UnicodeEncodeError:
    return unicode(value)


def _MakeXMLSafe(s):
  """Escapes <, >, and & from s, and removes XML 1.0-illegal chars."""
  s = cgi.escape(s)  # Escape <, >, and &
  # Remove characters that cannot appear in an XML 1.0 document
  # (http://www.w3.org/TR/REC-xml/#charsets).
  #
  # NOTE: if there are problems with current solution, one may move to
  # XML 1.1, which allows such chars, if they're entity-escaped (&#xHH;).
  s = re.sub(r'[\x00-\x08\x0b\x0c\x0e-\x1f]', '', s)
  # Convert non-ascii characters to entities.  Note: requires python >=2.3
  s = s.encode('ascii', 'xmlcharrefreplace')   # u'\xce\x88' -> 'u&#904;'
  return s


def _WriteSimpleXMLElement(outfile, name, value, indent):
  """Writes a simple XML element.

  Args:
    outfile: File object we write the XML element to.
    name: A string, the name of XML element.
    value: A Python object, whose string representation will be used
      as the value of the XML element.
    indent: A string, prepended to each line of generated output.
  """
  value_str = _StrOrUnicode(value)
  if isinstance(value, bool):
    # Display boolean values as the C++ flag library does: no caps.
    value_str = value_str.lower()
  safe_value_str = _MakeXMLSafe(value_str)
  outfile.write('%s<%s>%s</%s>\n' % (indent, name, safe_value_str, name))


class Flag:
  """Information about a command-line flag.

  'Flag' objects define the following fields:
    .name  - the name for this flag
    .default - the default value for this flag
    .default_as_str - default value as repr'd string, e.g., "'true'" (or None)
    .value  - the most recent parsed value of this flag; set by Parse()
    .help  - a help string or None if no help is available
    .short_name  - the single letter alias for this flag (or None)
    .boolean  - if 'true', this flag does not accept arguments
    .present  - true if this flag was parsed from command line flags.
    .parser  - an ArgumentParser object
    .serializer - an ArgumentSerializer object
    .allow_override - the flag may be redefined without raising an error

  The only public method of a 'Flag' object is Parse(), but it is
  typically only called by a 'FlagValues' object.  The Parse() method is
  a thin wrapper around the 'ArgumentParser' Parse() method.  The parsed
  value is saved in .value, and the .present attribute is updated.  If
  this flag was already present, a FlagsError is raised.

  Parse() is also called during __init__ to parse the default value and
  initialize the .value attribute.  This enables other python modules to
  safely use flags even if the __main__ module neglects to parse the
  command line arguments.  The .present attribute is cleared after
  __init__ parsing.  If the default value is set to None, then the
  __init__ parsing step is skipped and the .value attribute is
  initialized to None.

  Note: The default value is also presented to the user in the help
  string, so it is important that it be a legal value for this flag.
  """

  def __init__(self, parser, serializer, name, default, help_string,
               short_name=None, boolean=0, allow_override=0):
    self.name = name

    if not help_string:
      help_string = '(no help available)'

    self.help = help_string
    self.short_name = short_name
    self.boolean = boolean
    self.present = 0
    self.parser = parser
    self.serializer = serializer
    self.allow_override = allow_override
    self.value = None
    self.validators = []

    self.SetDefault(default)

  def __hash__(self):
    return hash(id(self))

  def __eq__(self, other):
    return self is other

  def __lt__(self, other):
    if isinstance(other, Flag):
      return id(self) < id(other)
    return NotImplemented

  def __GetParsedValueAsString(self, value):
    if value is None:
      return None
    if self.serializer:
      return repr(self.serializer.Serialize(value))
    if self.boolean:
      if value:
        return repr('true')
      else:
        return repr('false')
    return repr(_StrOrUnicode(value))

  def Parse(self, argument):
    try:
      self.value = self.parser.Parse(argument)
    except ValueError, e:  # recast ValueError as IllegalFlagValue
      raise IllegalFlagValue("flag --%s=%s: %s" % (self.name, argument, e))
    self.present += 1

  def Unparse(self):
    if self.default is None:
      self.value = None
    else:
      self.Parse(self.default)
    self.present = 0

  def Serialize(self):
    if self.value is None:
      return ''
    if self.boolean:
      if self.value:
        return "--%s" % self.name
      else:
        return "--no%s" % self.name
    else:
      if not self.serializer:
        raise FlagsError("Serializer not present for flag %s" % self.name)
      return "--%s=%s" % (self.name, self.serializer.Serialize(self.value))

  def SetDefault(self, value):
    """Changes the default value (and current value too) for this Flag."""
    # We can't allow a None override because it may end up not being
    # passed to C++ code when we're overriding C++ flags.  So we
    # cowardly bail out until someone fixes the semantics of trying to
    # pass None to a C++ flag.  See swig_flags.Init() for details on
    # this behavior.
    # TODO(olexiy): Users can directly call this method, bypassing all flags
    # validators (we don't have FlagValues here, so we can not check
    # validators).
    # The simplest solution I see is to make this method private.
    # Another approach would be to store reference to the corresponding
    # FlagValues with each flag, but this seems to be an overkill.
    if value is None and self.allow_override:
      raise DuplicateFlagCannotPropagateNoneToSwig(self.name)

    self.default = value
    self.Unparse()
    self.default_as_str = self.__GetParsedValueAsString(self.value)

  def Type(self):
    """Returns: a string that describes the type of this Flag."""
    # NOTE: we use strings, and not the types.*Type constants because
    # our flags can have more exotic types, e.g., 'comma separated list
    # of strings', 'whitespace separated list of strings', etc.
    return self.parser.Type()

  def WriteInfoInXMLFormat(self, outfile, module_name, is_key=False, indent=''):
    """Writes common info about this flag, in XML format.

    This is information that is relevant to all flags (e.g., name,
    meaning, etc.).  If you defined a flag that has some other pieces of
    info, then please override _WriteCustomInfoInXMLFormat.

    Please do NOT override this method.

    Args:
      outfile: File object we write to.
      module_name: A string, the name of the module that defines this flag.
      is_key: A boolean, True iff this flag is key for main module.
      indent: A string that is prepended to each generated line.
    """
    outfile.write(indent + '<flag>\n')
    inner_indent = indent + '  '
    if is_key:
      _WriteSimpleXMLElement(outfile, 'key', 'yes', inner_indent)
    _WriteSimpleXMLElement(outfile, 'file', module_name, inner_indent)
    # Print flag features that are relevant for all flags.
    _WriteSimpleXMLElement(outfile, 'name', self.name, inner_indent)
    if self.short_name:
      _WriteSimpleXMLElement(outfile, 'short_name', self.short_name,
                             inner_indent)
    if self.help:
      _WriteSimpleXMLElement(outfile, 'meaning', self.help, inner_indent)
    # The default flag value can either be represented as a string like on the
    # command line, or as a Python object.  We serialize this value in the
    # latter case in order to remain consistent.
    if self.serializer and not isinstance(self.default, str):
      default_serialized = self.serializer.Serialize(self.default)
    else:
      default_serialized = self.default
    _WriteSimpleXMLElement(outfile, 'default', default_serialized, inner_indent)
    _WriteSimpleXMLElement(outfile, 'current', self.value, inner_indent)
    _WriteSimpleXMLElement(outfile, 'type', self.Type(), inner_indent)
    # Print extra flag features this flag may have.
    self._WriteCustomInfoInXMLFormat(outfile, inner_indent)
    outfile.write(indent + '</flag>\n')

  def _WriteCustomInfoInXMLFormat(self, outfile, indent):
    """Writes extra info about this flag, in XML format.

    "Extra" means "not already printed by WriteInfoInXMLFormat above."

    Args:
      outfile: File object we write to.
      indent: A string that is prepended to each generated line.
    """
    # Usually, the parser knows the extra details about the flag, so
    # we just forward the call to it.
    self.parser.WriteCustomInfoInXMLFormat(outfile, indent)
# End of Flag definition


class _ArgumentParserCache(type):
  """Metaclass used to cache and share argument parsers among flags."""

  _instances = {}

  def __call__(mcs, *args, **kwargs):
    """Returns an instance of the argument parser cls.

    This method overrides behavior of the __new__ methods in
    all subclasses of ArgumentParser (inclusive). If an instance
    for mcs with the same set of arguments exists, this instance is
    returned, otherwise a new instance is created.

    If any keyword arguments are defined, or the values in args
    are not hashable, this method always returns a new instance of
    cls.

    Args:
      args: Positional initializer arguments.
      kwargs: Initializer keyword arguments.

    Returns:
      An instance of cls, shared or new.
    """
    if kwargs:
      return type.__call__(mcs, *args, **kwargs)
    else:
      instances = mcs._instances
      key = (mcs,) + tuple(args)
      try:
        return instances[key]
      except KeyError:
        # No cache entry for key exists, create a new one.
        return instances.setdefault(key, type.__call__(mcs, *args))
      except TypeError:
        # An object in args cannot be hashed, always return
        # a new instance.
        return type.__call__(mcs, *args)


class ArgumentParser(object):
  """Base class used to parse and convert arguments.

  The Parse() method checks to make sure that the string argument is a
  legal value and convert it to a native type.  If the value cannot be
  converted, it should throw a 'ValueError' exception with a human
  readable explanation of why the value is illegal.

  Subclasses should also define a syntactic_help string which may be
  presented to the user to describe the form of the legal values.

  Argument parser classes must be stateless, since instances are cached
  and shared between flags. Initializer arguments are allowed, but all
  member variables must be derived from initializer arguments only.
  """
  __metaclass__ = _ArgumentParserCache

  syntactic_help = ""

  def Parse(self, argument):
    """Default implementation: always returns its argument unmodified."""
    return argument

  def Type(self):
    return 'string'

  def WriteCustomInfoInXMLFormat(self, outfile, indent):
    pass


class ArgumentSerializer:
  """Base class for generating string representations of a flag value."""

  def Serialize(self, value):
    return _StrOrUnicode(value)


class ListSerializer(ArgumentSerializer):

  def __init__(self, list_sep):
    self.list_sep = list_sep

  def Serialize(self, value):
    return self.list_sep.join([_StrOrUnicode(x) for x in value])


# Flags validators


def RegisterValidator(flag_name,
                      checker,
                      message='Flag validation failed',
                      flag_values=FLAGS):
  """Adds a constraint, which will be enforced during program execution.

  The constraint is validated when flags are initially parsed, and after each
  change of the corresponding flag's value.
  Args:
    flag_name: string, name of the flag to be checked.
    checker: method to validate the flag.
      input  - value of the corresponding flag (string, boolean, etc.
        This value will be passed to checker by the library). See file's
        docstring for examples.
      output - Boolean.
        Must return True if validator constraint is satisfied.
        If constraint is not satisfied, it should either return False or
          raise gflags_validators.Error(desired_error_message).
    message: error text to be shown to the user if checker returns False.
      If checker raises gflags_validators.Error, message from the raised
        Error will be shown.
    flag_values: FlagValues
  Raises:
    AttributeError: if flag_name is not registered as a valid flag name.
  """
  flag_values.AddValidator(gflags_validators.SimpleValidator(flag_name,
                                                            checker,
                                                            message))


def MarkFlagAsRequired(flag_name, flag_values=FLAGS):
  """Ensure that flag is not None during program execution.

  Registers a flag validator, which will follow usual validator
  rules.
  Args:
    flag_name: string, name of the flag
    flag_values: FlagValues
  Raises:
    AttributeError: if flag_name is not registered as a valid flag name.
  """
  RegisterValidator(flag_name,
                    lambda value: value is not None,
                    message='Flag --%s must be specified.' % flag_name,
                    flag_values=flag_values)


def _RegisterBoundsValidatorIfNeeded(parser, name, flag_values):
  """Enforce lower and upper bounds for numeric flags.

  Args:
    parser: NumericParser (either FloatParser or IntegerParser). Provides lower
      and upper bounds, and help text to display.
    name: string, name of the flag
    flag_values: FlagValues
  """
  if parser.lower_bound is not None or parser.upper_bound is not None:

    def Checker(value):
      if value is not None and parser.IsOutsideBounds(value):
        message = '%s is not %s' % (value, parser.syntactic_help)
        raise gflags_validators.Error(message)
      return True

    RegisterValidator(name,
                      Checker,
                      flag_values=flag_values)


# The DEFINE functions are explained in mode details in the module doc string.


def DEFINE(parser, name, default, help, flag_values=FLAGS, serializer=None,
           **args):
  """Registers a generic Flag object.

  NOTE: in the docstrings of all DEFINE* functions, "registers" is short
  for "creates a new flag and registers it".

  Auxiliary function: clients should use the specialized DEFINE_<type>
  function instead.

  Args:
    parser: ArgumentParser that is used to parse the flag arguments.
    name: A string, the flag name.
    default: The default value of the flag.
    help: A help string.
    flag_values: FlagValues object the flag will be registered with.
    serializer: ArgumentSerializer that serializes the flag value.
    args: Dictionary with extra keyword args that are passes to the
      Flag __init__.
  """
  DEFINE_flag(Flag(parser, serializer, name, default, help, **args),
              flag_values)


def DEFINE_flag(flag, flag_values=FLAGS):
  """Registers a 'Flag' object with a 'FlagValues' object.

  By default, the global FLAGS 'FlagValue' object is used.

  Typical users will use one of the more specialized DEFINE_xxx
  functions, such as DEFINE_string or DEFINE_integer.  But developers
  who need to create Flag objects themselves should use this function
  to register their flags.
  """
  # copying the reference to flag_values prevents pychecker warnings
  fv = flag_values
  fv[flag.name] = flag
  # Tell flag_values who's defining the flag.
  if isinstance(flag_values, FlagValues):
    # Regarding the above isinstance test: some users pass funny
    # values of flag_values (e.g., {}) in order to avoid the flag
    # registration (in the past, there used to be a flag_values ==
    # FLAGS test here) and redefine flags with the same name (e.g.,
    # debug).  To avoid breaking their code, we perform the
    # registration only if flag_values is a real FlagValues object.
    module, module_name = _GetCallingModuleObjectAndName()
    flag_values._RegisterFlagByModule(module_name, flag)
    flag_values._RegisterFlagByModuleId(id(module), flag)


def _InternalDeclareKeyFlags(flag_names,
                             flag_values=FLAGS, key_flag_values=None):
  """Declares a flag as key for the calling module.

  Internal function.  User code should call DECLARE_key_flag or
  ADOPT_module_key_flags instead.

  Args:
    flag_names: A list of strings that are names of already-registered
      Flag objects.
    flag_values: A FlagValues object that the flags listed in
      flag_names have registered with (the value of the flag_values
      argument from the DEFINE_* calls that defined those flags).
      This should almost never need to be overridden.
    key_flag_values: A FlagValues object that (among possibly many
      other things) keeps track of the key flags for each module.
      Default None means "same as flag_values".  This should almost
      never need to be overridden.

  Raises:
    UnrecognizedFlagError: when we refer to a flag that was not
      defined yet.
  """
  key_flag_values = key_flag_values or flag_values

  module = _GetCallingModule()

  for flag_name in flag_names:
    if flag_name not in flag_values:
      raise UnrecognizedFlagError(flag_name)
    flag = flag_values.FlagDict()[flag_name]
    key_flag_values._RegisterKeyFlagForModule(module, flag)


def DECLARE_key_flag(flag_name, flag_values=FLAGS):
  """Declares one flag as key to the current module.

  Key flags are flags that are deemed really important for a module.
  They are important when listing help messages; e.g., if the
  --helpshort command-line flag is used, then only the key flags of the
  main module are listed (instead of all flags, as in the case of
  --help).

  Sample usage:

    gflags.DECLARED_key_flag('flag_1')

  Args:
    flag_name: A string, the name of an already declared flag.
      (Redeclaring flags as key, including flags implicitly key
      because they were declared in this module, is a no-op.)
    flag_values: A FlagValues object.  This should almost never
      need to be overridden.
  """
  if flag_name in _SPECIAL_FLAGS:
    # Take care of the special flags, e.g., --flagfile, --undefok.
    # These flags are defined in _SPECIAL_FLAGS, and are treated
    # specially during flag parsing, taking precedence over the
    # user-defined flags.
    _InternalDeclareKeyFlags([flag_name],
                             flag_values=_SPECIAL_FLAGS,
                             key_flag_values=flag_values)
    return
  _InternalDeclareKeyFlags([flag_name], flag_values=flag_values)


def ADOPT_module_key_flags(module, flag_values=FLAGS):
  """Declares that all flags key to a module are key to the current module.

  Args:
    module: A module object.
    flag_values: A FlagValues object.  This should almost never need
      to be overridden.

  Raises:
    FlagsError: When given an argument that is a module name (a
    string), instead of a module object.
  """
  # NOTE(salcianu): an even better test would be if not
  # isinstance(module, types.ModuleType) but I didn't want to import
  # types for such a tiny use.
  if isinstance(module, str):
    raise FlagsError('Received module name %s; expected a module object.'
                     % module)
  _InternalDeclareKeyFlags(
      [f.name for f in flag_values._GetKeyFlagsForModule(module.__name__)],
      flag_values=flag_values)
  # If module is this flag module, take _SPECIAL_FLAGS into account.
  if module == _GetThisModuleObjectAndName()[0]:
    _InternalDeclareKeyFlags(
        # As we associate flags with _GetCallingModuleObjectAndName(), the
        # special flags defined in this module are incorrectly registered with
        # a different module.  So, we can't use _GetKeyFlagsForModule.
        # Instead, we take all flags from _SPECIAL_FLAGS (a private
        # FlagValues, where no other module should register flags).
        [f.name for f in _SPECIAL_FLAGS.FlagDict().values()],
        flag_values=_SPECIAL_FLAGS,
        key_flag_values=flag_values)


#
# STRING FLAGS
#


def DEFINE_string(name, default, help, flag_values=FLAGS, **args):
  """Registers a flag whose value can be any string."""
  parser = ArgumentParser()
  serializer = ArgumentSerializer()
  DEFINE(parser, name, default, help, flag_values, serializer, **args)


#
# BOOLEAN FLAGS
#


class BooleanParser(ArgumentParser):
  """Parser of boolean values."""

  def Convert(self, argument):
    """Converts the argument to a boolean; raise ValueError on errors."""
    if type(argument) == str:
      if argument.lower() in ['true', 't', '1']:
        return True
      elif argument.lower() in ['false', 'f', '0']:
        return False

    bool_argument = bool(argument)
    if argument == bool_argument:
      # The argument is a valid boolean (True, False, 0, or 1), and not just
      # something that always converts to bool (list, string, int, etc.).
      return bool_argument

    raise ValueError('Non-boolean argument to boolean flag', argument)

  def Parse(self, argument):
    val = self.Convert(argument)
    return val

  def Type(self):
    return 'bool'


class BooleanFlag(Flag):
  """Basic boolean flag.

  Boolean flags do not take any arguments, and their value is either
  True (1) or False (0).  The false value is specified on the command
  line by prepending the word 'no' to either the long or the short flag
  name.

  For example, if a Boolean flag was created whose long name was
  'update' and whose short name was 'x', then this flag could be
  explicitly unset through either --noupdate or --nox.
  """

  def __init__(self, name, default, help, short_name=None, **args):
    p = BooleanParser()
    Flag.__init__(self, p, None, name, default, help, short_name, 1, **args)
    if not self.help: self.help = "a boolean value"


def DEFINE_boolean(name, default, help, flag_values=FLAGS, **args):
  """Registers a boolean flag.

  Such a boolean flag does not take an argument.  If a user wants to
  specify a false value explicitly, the long option beginning with 'no'
  must be used: i.e. --noflag

  This flag will have a value of None, True or False.  None is possible
  if default=None and the user does not specify the flag on the command
  line.
  """
  DEFINE_flag(BooleanFlag(name, default, help, **args), flag_values)


# Match C++ API to unconfuse C++ people.
DEFINE_bool = DEFINE_boolean


class HelpFlag(BooleanFlag):
  """
  HelpFlag is a special boolean flag that prints usage information and
  raises a SystemExit exception if it is ever found in the command
  line arguments.  Note this is called with allow_override=1, so other
  apps can define their own --help flag, replacing this one, if they want.
  """
  def __init__(self):
    BooleanFlag.__init__(self, "help", 0, "show this help",
                         short_name="?", allow_override=1)
  def Parse(self, arg):
    if arg:
      doc = sys.modules["__main__"].__doc__
      flags = str(FLAGS)
      print doc or ("\nUSAGE: %s [flags]\n" % sys.argv[0])
      if flags:
        print "flags:"
        print flags
      sys.exit(1)
class HelpXMLFlag(BooleanFlag):
  """Similar to HelpFlag, but generates output in XML format."""
  def __init__(self):
    BooleanFlag.__init__(self, 'helpxml', False,
                         'like --help, but generates XML output',
                         allow_override=1)
  def Parse(self, arg):
    if arg:
      FLAGS.WriteHelpInXMLFormat(sys.stdout)
      sys.exit(1)
class HelpshortFlag(BooleanFlag):
  """
  HelpshortFlag is a special boolean flag that prints usage
  information for the "main" module, and rasies a SystemExit exception
  if it is ever found in the command line arguments.  Note this is
  called with allow_override=1, so other apps can define their own
  --helpshort flag, replacing this one, if they want.
  """
  def __init__(self):
    BooleanFlag.__init__(self, "helpshort", 0,
                         "show usage only for this module", allow_override=1)
  def Parse(self, arg):
    if arg:
      doc = sys.modules["__main__"].__doc__
      flags = FLAGS.MainModuleHelp()
      print doc or ("\nUSAGE: %s [flags]\n" % sys.argv[0])
      if flags:
        print "flags:"
        print flags
      sys.exit(1)

#
# Numeric parser - base class for Integer and Float parsers
#


class NumericParser(ArgumentParser):
  """Parser of numeric values.

  Parsed value may be bounded to a given upper and lower bound.
  """

  def IsOutsideBounds(self, val):
    return ((self.lower_bound is not None and val < self.lower_bound) or
            (self.upper_bound is not None and val > self.upper_bound))

  def Parse(self, argument):
    val = self.Convert(argument)
    if self.IsOutsideBounds(val):
      raise ValueError("%s is not %s" % (val, self.syntactic_help))
    return val

  def WriteCustomInfoInXMLFormat(self, outfile, indent):
    if self.lower_bound is not None:
      _WriteSimpleXMLElement(outfile, 'lower_bound', self.lower_bound, indent)
    if self.upper_bound is not None:
      _WriteSimpleXMLElement(outfile, 'upper_bound', self.upper_bound, indent)

  def Convert(self, argument):
    """Default implementation: always returns its argument unmodified."""
    return argument

# End of Numeric Parser

#
# FLOAT FLAGS
#


class FloatParser(NumericParser):
  """Parser of floating point values.

  Parsed value may be bounded to a given upper and lower bound.
  """
  number_article = "a"
  number_name = "number"
  syntactic_help = " ".join((number_article, number_name))

  def __init__(self, lower_bound=None, upper_bound=None):
    super(FloatParser, self).__init__()
    self.lower_bound = lower_bound
    self.upper_bound = upper_bound
    sh = self.syntactic_help
    if lower_bound is not None and upper_bound is not None:
      sh = ("%s in the range [%s, %s]" % (sh, lower_bound, upper_bound))
    elif lower_bound == 0:
      sh = "a non-negative %s" % self.number_name
    elif upper_bound == 0:
      sh = "a non-positive %s" % self.number_name
    elif upper_bound is not None:
      sh = "%s <= %s" % (self.number_name, upper_bound)
    elif lower_bound is not None:
      sh = "%s >= %s" % (self.number_name, lower_bound)
    self.syntactic_help = sh

  def Convert(self, argument):
    """Converts argument to a float; raises ValueError on errors."""
    return float(argument)

  def Type(self):
    return 'float'
# End of FloatParser


def DEFINE_float(name, default, help, lower_bound=None, upper_bound=None,
                 flag_values=FLAGS, **args):
  """Registers a flag whose value must be a float.

  If lower_bound or upper_bound are set, then this flag must be
  within the given range.
  """
  parser = FloatParser(lower_bound, upper_bound)
  serializer = ArgumentSerializer()
  DEFINE(parser, name, default, help, flag_values, serializer, **args)
  _RegisterBoundsValidatorIfNeeded(parser, name, flag_values=flag_values)

#
# INTEGER FLAGS
#


class IntegerParser(NumericParser):
  """Parser of an integer value.

  Parsed value may be bounded to a given upper and lower bound.
  """
  number_article = "an"
  number_name = "integer"
  syntactic_help = " ".join((number_article, number_name))

  def __init__(self, lower_bound=None, upper_bound=None):
    super(IntegerParser, self).__init__()
    self.lower_bound = lower_bound
    self.upper_bound = upper_bound
    sh = self.syntactic_help
    if lower_bound is not None and upper_bound is not None:
      sh = ("%s in the range [%s, %s]" % (sh, lower_bound, upper_bound))
    elif lower_bound == 1:
      sh = "a positive %s" % self.number_name
    elif upper_bound == -1:
      sh = "a negative %s" % self.number_name
    elif lower_bound == 0:
      sh = "a non-negative %s" % self.number_name
    elif upper_bound == 0:
      sh = "a non-positive %s" % self.number_name
    elif upper_bound is not None:
      sh = "%s <= %s" % (self.number_name, upper_bound)
    elif lower_bound is not None:
      sh = "%s >= %s" % (self.number_name, lower_bound)
    self.syntactic_help = sh

  def Convert(self, argument):
    __pychecker__ = 'no-returnvalues'
    if type(argument) == str:
      base = 10
      if len(argument) > 2 and argument[0] == "0" and argument[1] == "x":
        base = 16
      return int(argument, base)
    else:
      return int(argument)

  def Type(self):
    return 'int'


def DEFINE_integer(name, default, help, lower_bound=None, upper_bound=None,
                   flag_values=FLAGS, **args):
  """Registers a flag whose value must be an integer.

  If lower_bound, or upper_bound are set, then this flag must be
  within the given range.
  """
  parser = IntegerParser(lower_bound, upper_bound)
  serializer = ArgumentSerializer()
  DEFINE(parser, name, default, help, flag_values, serializer, **args)
  _RegisterBoundsValidatorIfNeeded(parser, name, flag_values=flag_values)


#
# ENUM FLAGS
#


class EnumParser(ArgumentParser):
  """Parser of a string enum value (a string value from a given set).

  If enum_values (see below) is not specified, any string is allowed.
  """

  def __init__(self, enum_values=None):
    super(EnumParser, self).__init__()
    self.enum_values = enum_values

  def Parse(self, argument):
    if self.enum_values and argument not in self.enum_values:
      raise ValueError("value should be one of <%s>" %
                       "|".join(self.enum_values))
    return argument

  def Type(self):
    return 'string enum'


class EnumFlag(Flag):
  """Basic enum flag; its value can be any string from list of enum_values."""

  def __init__(self, name, default, help, enum_values=None,
               short_name=None, **args):
    enum_values = enum_values or []
    p = EnumParser(enum_values)
    g = ArgumentSerializer()
    Flag.__init__(self, p, g, name, default, help, short_name, **args)
    if not self.help: self.help = "an enum string"
    self.help = "<%s>: %s" % ("|".join(enum_values), self.help)

  def _WriteCustomInfoInXMLFormat(self, outfile, indent):
    for enum_value in self.parser.enum_values:
      _WriteSimpleXMLElement(outfile, 'enum_value', enum_value, indent)


def DEFINE_enum(name, default, enum_values, help, flag_values=FLAGS,
                **args):
  """Registers a flag whose value can be any string from enum_values."""
  DEFINE_flag(EnumFlag(name, default, help, enum_values, ** args),
              flag_values)


#
# LIST FLAGS
#


class BaseListParser(ArgumentParser):
  """Base class for a parser of lists of strings.

  To extend, inherit from this class; from the subclass __init__, call

    BaseListParser.__init__(self, token, name)

  where token is a character used to tokenize, and name is a description
  of the separator.
  """

  def __init__(self, token=None, name=None):
    assert name
    super(BaseListParser, self).__init__()
    self._token = token
    self._name = name
    self.syntactic_help = "a %s separated list" % self._name

  def Parse(self, argument):
    if isinstance(argument, list):
      return argument
    elif argument == '':
      return []
    else:
      return [s.strip() for s in argument.split(self._token)]

  def Type(self):
    return '%s separated list of strings' % self._name


class ListParser(BaseListParser):
  """Parser for a comma-separated list of strings."""

  def __init__(self):
    BaseListParser.__init__(self, ',', 'comma')

  def WriteCustomInfoInXMLFormat(self, outfile, indent):
    BaseListParser.WriteCustomInfoInXMLFormat(self, outfile, indent)
    _WriteSimpleXMLElement(outfile, 'list_separator', repr(','), indent)


class WhitespaceSeparatedListParser(BaseListParser):
  """Parser for a whitespace-separated list of strings."""

  def __init__(self):
    BaseListParser.__init__(self, None, 'whitespace')

  def WriteCustomInfoInXMLFormat(self, outfile, indent):
    BaseListParser.WriteCustomInfoInXMLFormat(self, outfile, indent)
    separators = list(string.whitespace)
    separators.sort()
    for ws_char in string.whitespace:
      _WriteSimpleXMLElement(outfile, 'list_separator', repr(ws_char), indent)


def DEFINE_list(name, default, help, flag_values=FLAGS, **args):
  """Registers a flag whose value is a comma-separated list of strings."""
  parser = ListParser()
  serializer = ListSerializer(',')
  DEFINE(parser, name, default, help, flag_values, serializer, **args)


def DEFINE_spaceseplist(name, default, help, flag_values=FLAGS, **args):
  """Registers a flag whose value is a whitespace-separated list of strings.

  Any whitespace can be used as a separator.
  """
  parser = WhitespaceSeparatedListParser()
  serializer = ListSerializer(' ')
  DEFINE(parser, name, default, help, flag_values, serializer, **args)


#
# MULTI FLAGS
#


class MultiFlag(Flag):
  """A flag that can appear multiple time on the command-line.

  The value of such a flag is a list that contains the individual values
  from all the appearances of that flag on the command-line.

  See the __doc__ for Flag for most behavior of this class.  Only
  differences in behavior are described here:

    * The default value may be either a single value or a list of values.
      A single value is interpreted as the [value] singleton list.

    * The value of the flag is always a list, even if the option was
      only supplied once, and even if the default value is a single
      value
  """

  def __init__(self, *args, **kwargs):
    Flag.__init__(self, *args, **kwargs)
    self.help += ';\n    repeat this option to specify a list of values'

  def Parse(self, arguments):
    """Parses one or more arguments with the installed parser.

    Args:
      arguments: a single argument or a list of arguments (typically a
        list of default values); a single argument is converted
        internally into a list containing one item.
    """
    if not isinstance(arguments, list):
      # Default value may be a list of values.  Most other arguments
      # will not be, so convert them into a single-item list to make
      # processing simpler below.
      arguments = [arguments]

    if self.present:
      # keep a backup reference to list of previously supplied option values
      values = self.value
    else:
      # "erase" the defaults with an empty list
      values = []

    for item in arguments:
      # have Flag superclass parse argument, overwriting self.value reference
      Flag.Parse(self, item)  # also increments self.present
      values.append(self.value)

    # put list of option values back in the 'value' attribute
    self.value = values

  def Serialize(self):
    if not self.serializer:
      raise FlagsError("Serializer not present for flag %s" % self.name)
    if self.value is None:
      return ''

    s = ''

    multi_value = self.value

    for self.value in multi_value:
      if s: s += ' '
      s += Flag.Serialize(self)

    self.value = multi_value

    return s

  def Type(self):
    return 'multi ' + self.parser.Type()


def DEFINE_multi(parser, serializer, name, default, help, flag_values=FLAGS,
                 **args):
  """Registers a generic MultiFlag that parses its args with a given parser.

  Auxiliary function.  Normal users should NOT use it directly.

  Developers who need to create their own 'Parser' classes for options
  which can appear multiple times can call this module function to
  register their flags.
  """
  DEFINE_flag(MultiFlag(parser, serializer, name, default, help, **args),
              flag_values)


def DEFINE_multistring(name, default, help, flag_values=FLAGS, **args):
  """Registers a flag whose value can be a list of any strings.

  Use the flag on the command line multiple times to place multiple
  string values into the list.  The 'default' may be a single string
  (which will be converted into a single-element list) or a list of
  strings.
  """
  parser = ArgumentParser()
  serializer = ArgumentSerializer()
  DEFINE_multi(parser, serializer, name, default, help, flag_values, **args)


def DEFINE_multi_int(name, default, help, lower_bound=None, upper_bound=None,
                     flag_values=FLAGS, **args):
  """Registers a flag whose value can be a list of arbitrary integers.

  Use the flag on the command line multiple times to place multiple
  integer values into the list.  The 'default' may be a single integer
  (which will be converted into a single-element list) or a list of
  integers.
  """
  parser = IntegerParser(lower_bound, upper_bound)
  serializer = ArgumentSerializer()
  DEFINE_multi(parser, serializer, name, default, help, flag_values, **args)


def DEFINE_multi_float(name, default, help, lower_bound=None, upper_bound=None,
                       flag_values=FLAGS, **args):
  """Registers a flag whose value can be a list of arbitrary floats.

  Use the flag on the command line multiple times to place multiple
  float values into the list.  The 'default' may be a single float
  (which will be converted into a single-element list) or a list of
  floats.
  """
  parser = FloatParser(lower_bound, upper_bound)
  serializer = ArgumentSerializer()
  DEFINE_multi(parser, serializer, name, default, help, flag_values, **args)


# Now register the flags that we want to exist in all applications.
# These are all defined with allow_override=1, so user-apps can use
# these flagnames for their own purposes, if they want.
DEFINE_flag(HelpFlag())
DEFINE_flag(HelpshortFlag())
DEFINE_flag(HelpXMLFlag())

# Define special flags here so that help may be generated for them.
# NOTE: Please do NOT use _SPECIAL_FLAGS from outside this module.
_SPECIAL_FLAGS = FlagValues()


DEFINE_string(
    'flagfile', "",
    "Insert flag definitions from the given file into the command line.",
    _SPECIAL_FLAGS)

DEFINE_string(
    'undefok', "",
    "comma-separated list of flag names that it is okay to specify "
    "on the command line even if the program does not define a flag "
    "with that name.  IMPORTANT: flags in this list that have "
    "arguments MUST use the --flag=value format.", _SPECIAL_FLAGS)
