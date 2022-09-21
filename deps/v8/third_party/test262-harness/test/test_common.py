#!/usr/bin/env python3

# Copyright 2014 by Sam Mikes.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

import unittest

import os

# add parent dir to search path
import sys
sys.path.append("src")


from _common import *

def slurpFile(name):
    with open('test/' + name) as f:
        contents = f.read()
    return contents


class TestOldParsing(unittest.TestCase):

    def test_test(self):
        pass

    def test_overview(self):
        name = 'fixtures/test262-old-headers.js'
        contents = slurpFile(name)
        record = convertDocString(contents)

        self.assertEqual("""The production Block { } in strict code can't contain function
declaration;""", record['commentary'])

        self.assertEqual("bestPractice/Sbp_A1_T1.js", record['path'])
        self.assertEqual("Trying to declare function at the Block statement",
                         record['description'])
        self.assertEqual("", record['onlyStrict'])
        self.assertEqual("SyntaxError", record['negative'])
        self.assertEqual("http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls",
                         record['bestPractice'])


class TestYAMLParsing(unittest.TestCase):

    def test_overview(self):
        name = 'fixtures/test262-yaml-headers.js'
        contents = slurpFile(name)
        record = convertDocString(contents)

        self.assertEqual("The production Block { } in strict code can't contain function declaration;\n", record['commentary'])

        self.assertEqual("Trying to declare function at the Block statement",
                         record['description'])
        self.assertEqual(['onlyStrict'], record['flags'])
        self.assertEqual("", record['onlyStrict'])
        self.assertEqual("SyntaxError", record['negative'])
        self.assertEqual("http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls",
                         record['bestPractice'])


if __name__ == '__main__':
    unittest.main()
