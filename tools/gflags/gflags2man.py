#!/usr/bin/env python

# Copyright (c) 2006, Google Inc.
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


"""gflags2man runs a Google flags base program and generates a man page.

Run the program, parse the output, and then format that into a man
page.

Usage:
  gflags2man <program> [program] ...
"""

# TODO(csilvers): work with windows paths (\) as well as unix (/)

# This may seem a bit of an end run, but it:  doesn't bloat flags, can
# support python/java/C++, supports older executables, and can be
# extended to other document formats.
# Inspired by help2man.



import os
import re
import sys
import stat
import time

import gflags

_VERSION = '0.1'


def _GetDefaultDestDir():
  home = os.environ.get('HOME', '')
  homeman = os.path.join(home, 'man', 'man1')
  if home and os.path.exists(homeman):
    return homeman
  else:
    return os.environ.get('TMPDIR', '/tmp')

FLAGS = gflags.FLAGS
gflags.DEFINE_string('dest_dir', _GetDefaultDestDir(),
                    'Directory to write resulting manpage to.'
                    ' Specify \'-\' for stdout')
gflags.DEFINE_string('help_flag', '--help',
                    'Option to pass to target program in to get help')
gflags.DEFINE_integer('v', 0, 'verbosity level to use for output')


_MIN_VALID_USAGE_MSG = 9         # if fewer lines than this, help is suspect


class Logging:
  """A super-simple logging class"""
  def error(self, msg): print >>sys.stderr, "ERROR: ", msg
  def warn(self, msg): print >>sys.stderr, "WARNING: ", msg
  def info(self, msg): print msg
  def debug(self, msg): self.vlog(1, msg)
  def vlog(self, level, msg):
    if FLAGS.v >= level: print msg
logging = Logging()
class App:
  def usage(self, shorthelp=0):
    print >>sys.stderr, __doc__
    print >>sys.stderr, "flags:"
    print >>sys.stderr, str(FLAGS)
  def run(self):
    main(sys.argv)
app = App()


def GetRealPath(filename):
  """Given an executable filename, find in the PATH or find absolute path.
  Args:
    filename  An executable filename (string)
  Returns:
    Absolute version of filename.
    None if filename could not be found locally, absolutely, or in PATH
  """
  if os.path.isabs(filename):                # already absolute
    return filename

  if filename.startswith('./') or  filename.startswith('../'): # relative
    return os.path.abspath(filename)

  path = os.getenv('PATH', '')
  for directory in path.split(':'):
    tryname = os.path.join(directory, filename)
    if os.path.exists(tryname):
      if not os.path.isabs(directory):  # relative directory
        return os.path.abspath(tryname)
      return tryname
  if os.path.exists(filename):
    return os.path.abspath(filename)
  return None                         # could not determine

class Flag(object):
  """The information about a single flag."""

  def __init__(self, flag_desc, help):
    """Create the flag object.
    Args:
      flag_desc  The command line forms this could take. (string)
      help       The help text (string)
    """
    self.desc = flag_desc               # the command line forms
    self.help = help                    # the help text
    self.default = ''                   # default value
    self.tips = ''                      # parsing/syntax tips


class ProgramInfo(object):
  """All the information gleaned from running a program with --help."""

  # Match a module block start, for python scripts --help
  # "goopy.logging:"
  module_py_re = re.compile(r'(\S.+):$')
  # match the start of a flag listing
  # " -v,--verbosity:  Logging verbosity"
  flag_py_re         = re.compile(r'\s+(-\S+):\s+(.*)$')
  # "   (default: '0')"
  flag_default_py_re = re.compile(r'\s+\(default:\s+\'(.*)\'\)$')
  # "   (an integer)"
  flag_tips_py_re    = re.compile(r'\s+\((.*)\)$')

  # Match a module block start, for c++ programs --help
  # "google/base/commandlineflags":
  module_c_re = re.compile(r'\s+Flags from (\S.+):$')
  # match the start of a flag listing
  # " -v,--verbosity:  Logging verbosity"
  flag_c_re         = re.compile(r'\s+(-\S+)\s+(.*)$')

  # Match a module block start, for java programs --help
  # "com.google.common.flags"
  module_java_re = re.compile(r'\s+Flags for (\S.+):$')
  # match the start of a flag listing
  # " -v,--verbosity:  Logging verbosity"
  flag_java_re         = re.compile(r'\s+(-\S+)\s+(.*)$')

  def __init__(self, executable):
    """Create object with executable.
    Args:
      executable  Program to execute (string)
    """
    self.long_name = executable
    self.name = os.path.basename(executable)  # name
    # Get name without extension (PAR files)
    (self.short_name, self.ext) = os.path.splitext(self.name)
    self.executable = GetRealPath(executable)  # name of the program
    self.output = []          # output from the program.  List of lines.
    self.desc = []            # top level description.  List of lines
    self.modules = {}         # { section_name(string), [ flags ] }
    self.module_list = []     # list of module names in their original order
    self.date = time.localtime(time.time())   # default date info

  def Run(self):
    """Run it and collect output.

    Returns:
      1 (true)   If everything went well.
      0 (false)  If there were problems.
    """
    if not self.executable:
      logging.error('Could not locate "%s"' % self.long_name)
      return 0

    finfo = os.stat(self.executable)
    self.date = time.localtime(finfo[stat.ST_MTIME])

    logging.info('Running: %s %s </dev/null 2>&1'
                 % (self.executable, FLAGS.help_flag))
    # --help output is often routed to stderr, so we combine with stdout.
    # Re-direct stdin to /dev/null to encourage programs that
    # don't understand --help to exit.
    (child_stdin, child_stdout_and_stderr) = os.popen4(
      [self.executable, FLAGS.help_flag])
    child_stdin.close()       # '</dev/null'
    self.output = child_stdout_and_stderr.readlines()
    child_stdout_and_stderr.close()
    if len(self.output) < _MIN_VALID_USAGE_MSG:
      logging.error('Error: "%s %s" returned only %d lines: %s'
                    % (self.name, FLAGS.help_flag,
                       len(self.output), self.output))
      return 0
    return 1

  def Parse(self):
    """Parse program output."""
    (start_line, lang) = self.ParseDesc()
    if start_line < 0:
      return
    if 'python' == lang:
      self.ParsePythonFlags(start_line)
    elif 'c' == lang:
      self.ParseCFlags(start_line)
    elif 'java' == lang:
      self.ParseJavaFlags(start_line)

  def ParseDesc(self, start_line=0):
    """Parse the initial description.

    This could be Python or C++.

    Returns:
      (start_line, lang_type)
        start_line  Line to start parsing flags on (int)
        lang_type   Either 'python' or 'c'
       (-1, '')  if the flags start could not be found
    """
    exec_mod_start = self.executable + ':'

    after_blank = 0
    start_line = 0             # ignore the passed-in arg for now (?)
    for start_line in range(start_line, len(self.output)): # collect top description
      line = self.output[start_line].rstrip()
      # Python flags start with 'flags:\n'
      if ('flags:' == line
          and len(self.output) > start_line+1
          and '' == self.output[start_line+1].rstrip()):
        start_line += 2
        logging.debug('Flags start (python): %s' % line)
        return (start_line, 'python')
      # SWIG flags just have the module name followed by colon.
      if exec_mod_start == line:
        logging.debug('Flags start (swig): %s' % line)
        return (start_line, 'python')
      # C++ flags begin after a blank line and with a constant string
      if after_blank and line.startswith('  Flags from '):
        logging.debug('Flags start (c): %s' % line)
        return (start_line, 'c')
      # java flags begin with a constant string
      if line == 'where flags are':
        logging.debug('Flags start (java): %s' % line)
        start_line += 2                        # skip "Standard flags:"
        return (start_line, 'java')

      logging.debug('Desc: %s' % line)
      self.desc.append(line)
      after_blank = (line == '')
    else:
      logging.warn('Never found the start of the flags section for "%s"!'
                   % self.long_name)
      return (-1, '')

  def ParsePythonFlags(self, start_line=0):
    """Parse python/swig style flags."""
    modname = None                      # name of current module
    modlist = []
    flag = None
    for line_num in range(start_line, len(self.output)): # collect flags
      line = self.output[line_num].rstrip()
      if not line:                      # blank
        continue

      mobj = self.module_py_re.match(line)
      if mobj:                          # start of a new module
        modname = mobj.group(1)
        logging.debug('Module: %s' % line)
        if flag:
          modlist.append(flag)
        self.module_list.append(modname)
        self.modules.setdefault(modname, [])
        modlist = self.modules[modname]
        flag = None
        continue

      mobj = self.flag_py_re.match(line)
      if mobj:                          # start of a new flag
        if flag:
          modlist.append(flag)
        logging.debug('Flag: %s' % line)
        flag = Flag(mobj.group(1),  mobj.group(2))
        continue

      if not flag:                    # continuation of a flag
        logging.error('Flag info, but no current flag "%s"' % line)
      mobj = self.flag_default_py_re.match(line)
      if mobj:                          # (default: '...')
        flag.default = mobj.group(1)
        logging.debug('Fdef: %s' % line)
        continue
      mobj = self.flag_tips_py_re.match(line)
      if mobj:                          # (tips)
        flag.tips = mobj.group(1)
        logging.debug('Ftip: %s' % line)
        continue
      if flag and flag.help:
        flag.help += line              # multiflags tack on an extra line
      else:
        logging.info('Extra: %s' % line)
    if flag:
      modlist.append(flag)

  def ParseCFlags(self, start_line=0):
    """Parse C style flags."""
    modname = None                      # name of current module
    modlist = []
    flag = None
    for line_num in range(start_line, len(self.output)):  # collect flags
      line = self.output[line_num].rstrip()
      if not line:                      # blank lines terminate flags
        if flag:                        # save last flag
          modlist.append(flag)
          flag = None
        continue

      mobj = self.module_c_re.match(line)
      if mobj:                          # start of a new module
        modname = mobj.group(1)
        logging.debug('Module: %s' % line)
        if flag:
          modlist.append(flag)
        self.module_list.append(modname)
        self.modules.setdefault(modname, [])
        modlist = self.modules[modname]
        flag = None
        continue

      mobj = self.flag_c_re.match(line)
      if mobj:                          # start of a new flag
        if flag:                        # save last flag
          modlist.append(flag)
        logging.debug('Flag: %s' % line)
        flag = Flag(mobj.group(1),  mobj.group(2))
        continue

      # append to flag help.  type and default are part of the main text
      if flag:
        flag.help += ' ' + line.strip()
      else:
        logging.info('Extra: %s' % line)
    if flag:
      modlist.append(flag)

  def ParseJavaFlags(self, start_line=0):
    """Parse Java style flags (com.google.common.flags)."""
    # The java flags prints starts with a "Standard flags" "module"
    # that doesn't follow the standard module syntax.
    modname = 'Standard flags'          # name of current module
    self.module_list.append(modname)
    self.modules.setdefault(modname, [])
    modlist = self.modules[modname]
    flag = None

    for line_num in range(start_line, len(self.output)): # collect flags
      line = self.output[line_num].rstrip()
      logging.vlog(2, 'Line: "%s"' % line)
      if not line:                      # blank lines terminate module
        if flag:                        # save last flag
          modlist.append(flag)
          flag = None
        continue

      mobj = self.module_java_re.match(line)
      if mobj:                          # start of a new module
        modname = mobj.group(1)
        logging.debug('Module: %s' % line)
        if flag:
          modlist.append(flag)
        self.module_list.append(modname)
        self.modules.setdefault(modname, [])
        modlist = self.modules[modname]
        flag = None
        continue

      mobj = self.flag_java_re.match(line)
      if mobj:                          # start of a new flag
        if flag:                        # save last flag
          modlist.append(flag)
        logging.debug('Flag: %s' % line)
        flag = Flag(mobj.group(1),  mobj.group(2))
        continue

      # append to flag help.  type and default are part of the main text
      if flag:
        flag.help += ' ' + line.strip()
      else:
        logging.info('Extra: %s' % line)
    if flag:
      modlist.append(flag)

  def Filter(self):
    """Filter parsed data to create derived fields."""
    if not self.desc:
      self.short_desc = ''
      return

    for i in range(len(self.desc)):   # replace full path with name
      if self.desc[i].find(self.executable) >= 0:
        self.desc[i] = self.desc[i].replace(self.executable, self.name)

    self.short_desc = self.desc[0]
    word_list = self.short_desc.split(' ')
    all_names = [ self.name, self.short_name, ]
    # Since the short_desc is always listed right after the name,
    #  trim it from the short_desc
    while word_list and (word_list[0] in all_names
                         or word_list[0].lower() in all_names):
      del word_list[0]
      self.short_desc = ''              # signal need to reconstruct
    if not self.short_desc and word_list:
      self.short_desc = ' '.join(word_list)


class GenerateDoc(object):
  """Base class to output flags information."""

  def __init__(self, proginfo, directory='.'):
    """Create base object.
    Args:
      proginfo   A ProgramInfo object
      directory  Directory to write output into
    """
    self.info = proginfo
    self.dirname = directory

  def Output(self):
    """Output all sections of the page."""
    self.Open()
    self.Header()
    self.Body()
    self.Footer()

  def Open(self): raise NotImplementedError    # define in subclass
  def Header(self): raise NotImplementedError  # define in subclass
  def Body(self): raise NotImplementedError    # define in subclass
  def Footer(self): raise NotImplementedError  # define in subclass


class GenerateMan(GenerateDoc):
  """Output a man page."""

  def __init__(self, proginfo, directory='.'):
    """Create base object.
    Args:
      proginfo   A ProgramInfo object
      directory  Directory to write output into
    """
    GenerateDoc.__init__(self, proginfo, directory)

  def Open(self):
    if self.dirname == '-':
      logging.info('Writing to stdout')
      self.fp = sys.stdout
    else:
      self.file_path = '%s.1' % os.path.join(self.dirname, self.info.name)
      logging.info('Writing: %s' % self.file_path)
      self.fp = open(self.file_path, 'w')

  def Header(self):
    self.fp.write(
      '.\\" DO NOT MODIFY THIS FILE!  It was generated by gflags2man %s\n'
      % _VERSION)
    self.fp.write(
      '.TH %s "1" "%s" "%s" "User Commands"\n'
      % (self.info.name, time.strftime('%x', self.info.date), self.info.name))
    self.fp.write(
      '.SH NAME\n%s \\- %s\n' % (self.info.name, self.info.short_desc))
    self.fp.write(
      '.SH SYNOPSIS\n.B %s\n[\\fIFLAGS\\fR]...\n' % self.info.name)

  def Body(self):
    self.fp.write(
      '.SH DESCRIPTION\n.\\" Add any additional description here\n.PP\n')
    for ln in self.info.desc:
      self.fp.write('%s\n' % ln)
    self.fp.write(
      '.SH OPTIONS\n')
    # This shows flags in the original order
    for modname in self.info.module_list:
      if modname.find(self.info.executable) >= 0:
        mod = modname.replace(self.info.executable, self.info.name)
      else:
        mod = modname
      self.fp.write('\n.P\n.I %s\n' % mod)
      for flag in self.info.modules[modname]:
        help_string = flag.help
        if flag.default or flag.tips:
          help_string += '\n.br\n'
        if flag.default:
          help_string += '  (default: \'%s\')' % flag.default
        if flag.tips:
          help_string += '  (%s)' % flag.tips
        self.fp.write(
          '.TP\n%s\n%s\n' % (flag.desc, help_string))

  def Footer(self):
    self.fp.write(
      '.SH COPYRIGHT\nCopyright \(co %s Google.\n'
      % time.strftime('%Y', self.info.date))
    self.fp.write('Gflags2man created this page from "%s %s" output.\n'
                  % (self.info.name, FLAGS.help_flag))
    self.fp.write('\nGflags2man was written by Dan Christian. '
                  ' Note that the date on this'
                  ' page is the modification date of %s.\n' % self.info.name)


def main(argv):
  argv = FLAGS(argv)           # handles help as well
  if len(argv) <= 1:
    app.usage(shorthelp=1)
    return 1

  for arg in argv[1:]:
    prog = ProgramInfo(arg)
    if not prog.Run():
      continue
    prog.Parse()
    prog.Filter()
    doc = GenerateMan(prog, FLAGS.dest_dir)
    doc.Output()
  return 0

if __name__ == '__main__':
  app.run()
