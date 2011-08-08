#!/usr/bin/python2.4

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Visual Studio project reader/writer."""

import common
import xml.dom
import xml_fix


#------------------------------------------------------------------------------


class Writer(object):
  """Visual Studio XML tool file writer."""

  def __init__(self, tool_file_path):
    """Initializes the tool file.

    Args:
      tool_file_path: Path to the tool file.
    """
    self.tool_file_path = tool_file_path
    self.doc = None

  def Create(self, name):
    """Creates the tool file document.

    Args:
      name: Name of the tool file.
    """
    self.name = name

    # Create XML doc
    xml_impl = xml.dom.getDOMImplementation()
    self.doc = xml_impl.createDocument(None, 'VisualStudioToolFile', None)

    # Add attributes to root element
    self.n_root = self.doc.documentElement
    self.n_root.setAttribute('Version', '8.00')
    self.n_root.setAttribute('Name', self.name)

    # Add rules section
    self.n_rules = self.doc.createElement('Rules')
    self.n_root.appendChild(self.n_rules)

  def AddCustomBuildRule(self, name, cmd, description,
                         additional_dependencies,
                         outputs, extensions):
    """Adds a rule to the tool file.

    Args:
      name: Name of the rule.
      description: Description of the rule.
      cmd: Command line of the rule.
      additional_dependencies: other files which may trigger the rule.
      outputs: outputs of the rule.
      extensions: extensions handled by the rule.
    """
    n_rule = self.doc.createElement('CustomBuildRule')
    n_rule.setAttribute('Name', name)
    n_rule.setAttribute('ExecutionDescription', description)
    n_rule.setAttribute('CommandLine', cmd)
    n_rule.setAttribute('Outputs', ';'.join(outputs))
    n_rule.setAttribute('FileExtensions', ';'.join(extensions))
    n_rule.setAttribute('AdditionalDependencies',
                        ';'.join(additional_dependencies))
    self.n_rules.appendChild(n_rule)

  def Write(self, writer=common.WriteOnDiff):
    """Writes the tool file."""
    f = writer(self.tool_file_path)
    fix = xml_fix.XmlFix()
    self.doc.writexml(f, encoding='Windows-1252', addindent='  ', newl='\r\n')
    fix.Cleanup()
    f.close()

#------------------------------------------------------------------------------
