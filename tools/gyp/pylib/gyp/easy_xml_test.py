#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Unit tests for the easy_xml.py file. """

import easy_xml
import unittest
import StringIO


class TestSequenceFunctions(unittest.TestCase):

  def setUp(self):
    self.stderr = StringIO.StringIO()

  def test_EasyXml_simple(self):
    xml = easy_xml.EasyXml('test')
    self.assertEqual(str(xml), '<?xml version="1.0" ?><test/>')

  def test_EasyXml_simple_with_attributes(self):
    xml = easy_xml.EasyXml('test2', {'a': 'value1', 'b': 'value2'})
    self.assertEqual(str(xml),
                     '<?xml version="1.0" ?><test2 a="value1" b="value2"/>')

  def test_EasyXml_add_node(self):
    # We want to create:
    target = ('<?xml version="1.0" ?>'
        '<test3>'
          '<GrandParent>'
            '<Parent1>'
               '<Child/>'
            '</Parent1>'
            '<Parent2/>'
          '</GrandParent>'
        '</test3>')

    # Do it the hard way first:
    xml = easy_xml.EasyXml('test3')
    grand_parent = xml.AppendNode(xml.Root(), ['GrandParent'])
    parent1 = xml.AppendNode(grand_parent, ['Parent1'])
    parent2 = xml.AppendNode(grand_parent, ['Parent2'])
    xml.AppendNode(parent1, ['Child'])
    self.assertEqual(str(xml), target)

    # Do it the easier way:
    xml = easy_xml.EasyXml('test3')
    xml.AppendNode(xml.Root(),
        ['GrandParent',
            ['Parent1', ['Child']],
            ['Parent2']])
    self.assertEqual(str(xml), target)

  def test_EasyXml_complex(self):
    # We want to create:
    target = ('<?xml version="1.0" ?>'
        '<Project>'
          '<PropertyGroup Label="Globals">'
            '<ProjectGuid>{D2250C20-3A94-4FB9-AF73-11BC5B73884B}</ProjectGuid>'
            '<Keyword>Win32Proj</Keyword>'
            '<RootNamespace>automated_ui_tests</RootNamespace>'
          '</PropertyGroup>'
          '<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props"/>'
          '<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\''
                                      'Debug|Win32\'" Label="Configuration">'
            '<ConfigurationType>Application</ConfigurationType>'
            '<CharacterSet>Unicode</CharacterSet>'
          '</PropertyGroup>'
        '</Project>')

    xml = easy_xml.EasyXml('Project')
    xml.AppendChildren(xml.Root(), [
        ['PropertyGroup', {'Label': 'Globals'},
            ['ProjectGuid', '{D2250C20-3A94-4FB9-AF73-11BC5B73884B}'],
            ['Keyword', 'Win32Proj'],
            ['RootNamespace', 'automated_ui_tests']
        ],
        ['Import', {'Project': '$(VCTargetsPath)\\Microsoft.Cpp.props'}],
        ['PropertyGroup',
            {'Condition': "'$(Configuration)|$(Platform)'=='Debug|Win32'",
                'Label': 'Configuration'},
            ['ConfigurationType', 'Application'],
            ['CharacterSet', 'Unicode']
        ]
    ])
    self.assertEqual(str(xml), target)


if __name__ == '__main__':
  unittest.main()
