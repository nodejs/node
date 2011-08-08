#!/usr/bin/python2.4

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Visual Studio user preferences file writer."""

import common
import os
import re
import socket # for gethostname
import xml.dom
import xml_fix


#------------------------------------------------------------------------------

def _FindCommandInPath(command):
  """If there are no slashes in the command given, this function
     searches the PATH env to find the given command, and converts it
     to an absolute path.  We have to do this because MSVS is looking
     for an actual file to launch a debugger on, not just a command
     line.  Note that this happens at GYP time, so anything needing to
     be built needs to have a full path."""
  if '/' in command or '\\' in command:
    # If the command already has path elements (either relative or
    # absolute), then assume it is constructed properly.
    return command
  else:
    # Search through the path list and find an existing file that
    # we can access.
    paths = os.environ.get('PATH','').split(os.pathsep)
    for path in paths:
      item = os.path.join(path, command)
      if os.path.isfile(item) and os.access(item, os.X_OK):
        return item
  return command

def _QuoteWin32CommandLineArgs(args):
  new_args = []
  for arg in args:
    # Replace all double-quotes with double-double-quotes to escape
    # them for cmd shell, and then quote the whole thing if there
    # are any.
    if arg.find('"') != -1:
      arg = '""'.join(arg.split('"'))
      arg = '"%s"' % arg

    # Otherwise, if there are any spaces, quote the whole arg.
    elif re.search(r'[ \t\n]', arg):
      arg = '"%s"' % arg
    new_args.append(arg)
  return new_args

class Writer(object):
  """Visual Studio XML user user file writer."""

  def __init__(self, user_file_path, version):
    """Initializes the user file.

    Args:
      user_file_path: Path to the user file.
    """
    self.user_file_path = user_file_path
    self.version = version
    self.doc = None

  def Create(self, name):
    """Creates the user file document.

    Args:
      name: Name of the user file.
    """
    self.name = name

    # Create XML doc
    xml_impl = xml.dom.getDOMImplementation()
    self.doc = xml_impl.createDocument(None, 'VisualStudioUserFile', None)

    # Add attributes to root element
    self.n_root = self.doc.documentElement
    self.n_root.setAttribute('Version', self.version.ProjectVersion())
    self.n_root.setAttribute('Name', self.name)

    # Add configurations section
    self.n_configs = self.doc.createElement('Configurations')
    self.n_root.appendChild(self.n_configs)

  def _AddConfigToNode(self, parent, config_type, config_name):
    """Adds a configuration to the parent node.

    Args:
      parent: Destination node.
      config_type: Type of configuration node.
      config_name: Configuration name.
    """
    # Add configuration node and its attributes
    n_config = self.doc.createElement(config_type)
    n_config.setAttribute('Name', config_name)
    parent.appendChild(n_config)

  def AddConfig(self, name):
    """Adds a configuration to the project.

    Args:
      name: Configuration name.
    """
    self._AddConfigToNode(self.n_configs, 'Configuration', name)


  def AddDebugSettings(self, config_name, command, environment = {},
                       working_directory=""):
    """Adds a DebugSettings node to the user file for a particular config.

    Args:
      command: command line to run.  First element in the list is the
        executable.  All elements of the command will be quoted if
        necessary.
      working_directory: other files which may trigger the rule. (optional)
    """
    command = _QuoteWin32CommandLineArgs(command)

    n_cmd = self.doc.createElement('DebugSettings')
    abs_command = _FindCommandInPath(command[0])
    n_cmd.setAttribute('Command', abs_command)
    n_cmd.setAttribute('WorkingDirectory', working_directory)
    n_cmd.setAttribute('CommandArguments', " ".join(command[1:]))
    n_cmd.setAttribute('RemoteMachine', socket.gethostname())

    if environment and isinstance(environment, dict):
      n_cmd.setAttribute('Environment',
                         " ".join(['%s="%s"' % (key, val)
                                   for (key,val) in environment.iteritems()]))
    else:
      n_cmd.setAttribute('Environment', '')

    n_cmd.setAttribute('EnvironmentMerge', 'true')

    # Currently these are all "dummy" values that we're just setting
    # in the default manner that MSVS does it.  We could use some of
    # these to add additional capabilities, I suppose, but they might
    # not have parity with other platforms then.
    n_cmd.setAttribute('Attach', 'false')
    n_cmd.setAttribute('DebuggerType', '3') # 'auto' debugger
    n_cmd.setAttribute('Remote', '1')
    n_cmd.setAttribute('RemoteCommand', '')
    n_cmd.setAttribute('HttpUrl', '')
    n_cmd.setAttribute('PDBPath', '')
    n_cmd.setAttribute('SQLDebugging', '')
    n_cmd.setAttribute('DebuggerFlavor', '0')
    n_cmd.setAttribute('MPIRunCommand', '')
    n_cmd.setAttribute('MPIRunArguments', '')
    n_cmd.setAttribute('MPIRunWorkingDirectory', '')
    n_cmd.setAttribute('ApplicationCommand', '')
    n_cmd.setAttribute('ApplicationArguments', '')
    n_cmd.setAttribute('ShimCommand', '')
    n_cmd.setAttribute('MPIAcceptMode', '')
    n_cmd.setAttribute('MPIAcceptFilter', '')

    # Find the config, and add it if it doesn't exist.
    found = False
    for config in self.n_configs.childNodes:
      if config.getAttribute("Name") == config_name:
        found = True

    if not found:
      self.AddConfig(config_name)

    # Add the DebugSettings onto the appropriate config.
    for config in self.n_configs.childNodes:
      if config.getAttribute("Name") == config_name:
        config.appendChild(n_cmd)
        break

  def Write(self, writer=common.WriteOnDiff):
    """Writes the user file."""
    f = writer(self.user_file_path)
    self.doc.writexml(f, encoding='Windows-1252', addindent='  ', newl='\r\n')
    f.close()

#------------------------------------------------------------------------------
