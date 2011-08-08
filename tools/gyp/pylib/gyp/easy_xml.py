#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import xml.dom
import xml_fix
import common

class EasyXml(object):
  """ Class to easily create XML files with substantial pre-defined structures.

  Visual Studio files have a lot of pre-defined structures.  This class makes
  it easy to represent these structures as Python data structures, instead of
  having to create a lot of function calls.

  For this class, an XML element is represented as a list composed of:
  1. The name of the element, a string,
  2. The attributes of the element, an dictionary (optional), and
  3+. The content of the element, if any.  Strings are simple text nodes and
      lists are child elements.

  Example 1:
      <test/>
  becomes
      ['test']

  Example 2:
      <myelement a='value1' b='value2'>
         <childtype>This is</childtype>
         <childtype>it!</childtype>
      </myelement>

  becomes
      ['myelement', {'a':'value1', 'b':'value2'},
         ['childtype', 'This is'],
         ['childtype', 'it!'],
      ]
  """

  def __init__(self, name, attributes=None):
    """ Constructs an object representing an XML document.

    Args:
      name:  A string, the name of the root element.
      attributes:  A dictionary, the attributes of the root.
    """
    xml_impl = xml.dom.getDOMImplementation()
    self.doc = xml_impl.createDocument(None, name, None)
    if attributes:
      self.SetAttributes(self.doc.documentElement, attributes)

  def AppendChildren(self, parent, children_specifications):
    """ Appends multiple children.

    Args:
      parent:  The node to which the children will be added.
      children_specifications:  A list of node specifications.
    """
    for specification in children_specifications:
      # If it's a string, append a text node.
      # Otherwise append an XML node.
      if isinstance(specification, str):
        parent.appendChild(self.doc.createTextNode(specification))
      else:
        self.AppendNode(parent, specification)

  def AppendNode(self, parent, specification):
    """ Appends multiple children.

    Args:
      parent:  The node to which the child will be added.
      children_specifications:  A list, the node specification.  The first
          entry is the name of the element.  If the second entry is a
          dictionary, it is the attributes.  The remaining entries of the
          list are the sub-elements.
    Returns:
      The XML element created.
    """
    name = specification[0]
    if not isinstance(name, str):
      raise Exception('The first item of an EasyXml specification should be '
                      'a string.  Specification was ' + str(specification))
    element = self.doc.createElement(name)
    parent.appendChild(element)
    rest = specification[1:]
    # The second element is optionally a dictionary of the attributes.
    if rest and isinstance(rest[0], dict):
      self.SetAttributes(element, rest[0])
      rest = rest[1:]
    if rest:
      self.AppendChildren(element, rest)
    return element

  def SetAttributes(self, element, attribute_description):
    """ Sets the attributes of an element.

    Args:
      element:  The node to which the child will be added.
      attribute_description:  A dictionary that maps attribute names to
          attribute values.
    """
    for attribute, value in attribute_description.iteritems():
      element.setAttribute(attribute, value)

  def Root(self):
    """ Returns the root element. """
    return self.doc.documentElement

  def WriteIfChanged(self, path):
    """ Writes the XML doc but don't touch the file if unchanged. """
    f = common.WriteOnDiff(path)
    fix = xml_fix.XmlFix()
    self.doc.writexml(f, encoding='utf-8', addindent='', newl='')
    fix.Cleanup()
    f.close()

  def __str__(self):
    """ Converts the doc to a string. """
    return self.doc.toxml()
