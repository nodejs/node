#!/usr/bin/python2.4

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Visual Studio project reader/writer."""

import common
import xml.dom
import xml_fix

#------------------------------------------------------------------------------


class Tool(object):
  """Visual Studio tool."""

  def __init__(self, name, attrs=None):
    """Initializes the tool.

    Args:
      name: Tool name.
      attrs: Dict of tool attributes; may be None.
    """
    self.name = name
    self.attrs = attrs or {}

  def CreateElement(self, doc):
    """Creates an element for the tool.

    Args:
      doc: xml.dom.Document object to use for node creation.

    Returns:
      A new xml.dom.Element for the tool.
    """
    node = doc.createElement('Tool')
    node.setAttribute('Name', self.name)
    for k, v in self.attrs.items():
      node.setAttribute(k, v)
    return node


class Filter(object):
  """Visual Studio filter - that is, a virtual folder."""

  def __init__(self, name, contents=None):
    """Initializes the folder.

    Args:
      name: Filter (folder) name.
      contents: List of filenames and/or Filter objects contained.
    """
    self.name = name
    self.contents = list(contents or [])


#------------------------------------------------------------------------------


class Writer(object):
  """Visual Studio XML project writer."""

  def __init__(self, project_path, version):
    """Initializes the project.

    Args:
      project_path: Path to the project file.
      version: Format version to emit.
    """
    self.project_path = project_path
    self.doc = None
    self.version = version

  def Create(self, name, guid=None, platforms=None):
    """Creates the project document.

    Args:
      name: Name of the project.
      guid: GUID to use for project, if not None.
    """
    self.name = name
    self.guid = guid

    # Default to Win32 for platforms.
    if not platforms:
      platforms = ['Win32']

    # Create XML doc
    xml_impl = xml.dom.getDOMImplementation()
    self.doc = xml_impl.createDocument(None, 'VisualStudioProject', None)

    # Add attributes to root element
    self.n_root = self.doc.documentElement
    self.n_root.setAttribute('ProjectType', 'Visual C++')
    self.n_root.setAttribute('Version', self.version.ProjectVersion())
    self.n_root.setAttribute('Name', self.name)
    self.n_root.setAttribute('ProjectGUID', self.guid)
    self.n_root.setAttribute('RootNamespace', self.name)
    self.n_root.setAttribute('Keyword', 'Win32Proj')

    # Add platform list
    n_platform = self.doc.createElement('Platforms')
    self.n_root.appendChild(n_platform)
    for platform in platforms:
      n = self.doc.createElement('Platform')
      n.setAttribute('Name', platform)
      n_platform.appendChild(n)

    # Add tool files section
    self.n_tool_files = self.doc.createElement('ToolFiles')
    self.n_root.appendChild(self.n_tool_files)

    # Add configurations section
    self.n_configs = self.doc.createElement('Configurations')
    self.n_root.appendChild(self.n_configs)

    # Add empty References section
    self.n_root.appendChild(self.doc.createElement('References'))

    # Add files section
    self.n_files = self.doc.createElement('Files')
    self.n_root.appendChild(self.n_files)
    # Keep a dict keyed on filename to speed up access.
    self.n_files_dict = dict()

    # Add empty Globals section
    self.n_root.appendChild(self.doc.createElement('Globals'))

  def AddToolFile(self, path):
    """Adds a tool file to the project.

    Args:
      path: Relative path from project to tool file.
    """
    n_tool = self.doc.createElement('ToolFile')
    n_tool.setAttribute('RelativePath', path)
    self.n_tool_files.appendChild(n_tool)

  def _AddConfigToNode(self, parent, config_type, config_name, attrs=None,
                       tools=None):
    """Adds a configuration to the parent node.

    Args:
      parent: Destination node.
      config_type: Type of configuration node.
      config_name: Configuration name.
      attrs: Dict of configuration attributes; may be None.
      tools: List of tools (strings or Tool objects); may be None.
    """
    # Handle defaults
    if not attrs:
      attrs = {}
    if not tools:
      tools = []

    # Add configuration node and its attributes
    n_config = self.doc.createElement(config_type)
    n_config.setAttribute('Name', config_name)
    for k, v in attrs.items():
      n_config.setAttribute(k, v)
    parent.appendChild(n_config)

    # Add tool nodes and their attributes
    if tools:
      for t in tools:
        if isinstance(t, Tool):
          n_config.appendChild(t.CreateElement(self.doc))
        else:
          n_config.appendChild(Tool(t).CreateElement(self.doc))

  def AddConfig(self, name, attrs=None, tools=None):
    """Adds a configuration to the project.

    Args:
      name: Configuration name.
      attrs: Dict of configuration attributes; may be None.
      tools: List of tools (strings or Tool objects); may be None.
    """
    self._AddConfigToNode(self.n_configs, 'Configuration', name, attrs, tools)

  def _AddFilesToNode(self, parent, files):
    """Adds files and/or filters to the parent node.

    Args:
      parent: Destination node
      files: A list of Filter objects and/or relative paths to files.

    Will call itself recursively, if the files list contains Filter objects.
    """
    for f in files:
      if isinstance(f, Filter):
        node = self.doc.createElement('Filter')
        node.setAttribute('Name', f.name)
        self._AddFilesToNode(node, f.contents)
      else:
        node = self.doc.createElement('File')
        node.setAttribute('RelativePath', f)
        self.n_files_dict[f] = node
      parent.appendChild(node)

  def AddFiles(self, files):
    """Adds files to the project.

    Args:
      files: A list of Filter objects and/or relative paths to files.

    This makes a copy of the file/filter tree at the time of this call.  If you
    later add files to a Filter object which was passed into a previous call
    to AddFiles(), it will not be reflected in this project.
    """
    self._AddFilesToNode(self.n_files, files)
    # TODO(rspangler) This also doesn't handle adding files to an existing
    # filter.  That is, it doesn't merge the trees.

  def AddFileConfig(self, path, config, attrs=None, tools=None):
    """Adds a configuration to a file.

    Args:
      path: Relative path to the file.
      config: Name of configuration to add.
      attrs: Dict of configuration attributes; may be None.
      tools: List of tools (strings or Tool objects); may be None.

    Raises:
      ValueError: Relative path does not match any file added via AddFiles().
    """
    # Find the file node with the right relative path
    parent = self.n_files_dict.get(path)
    if not parent:
      raise ValueError('AddFileConfig: file "%s" not in project.' % path)

    # Add the config to the file node
    self._AddConfigToNode(parent, 'FileConfiguration', config, attrs, tools)

  def Write(self, writer=common.WriteOnDiff):
    """Writes the project file."""
    f = writer(self.project_path)
    fix = xml_fix.XmlFix()
    self.doc.writexml(f, encoding='Windows-1252', addindent='  ', newl='\r\n')
    fix.Cleanup()
    f.close()

#------------------------------------------------------------------------------
