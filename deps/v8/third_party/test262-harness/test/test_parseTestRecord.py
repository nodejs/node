#!/usr/bin/env python

# Copyright 2014 by Sam Mikes.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

import unittest

import os
import yaml

# add parent dir to search path
import sys
sys.path.append("src")

from parseTestRecord import *

def slurpFile(name):
    with open('test/' + name) as f:
        contents = f.read()
    return contents


class TestOldParsing(unittest.TestCase):

    def test_test(self):
        self.assertTrue(True)

    def test_overview(self):
        name = 'fixtures/test262-old-headers.js'
        contents = slurpFile(name)
        record = parseTestRecord(contents, name)

        self.assertEqual("""// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.""",
                         record['header'])
        self.assertEqual("""The production Block { } in strict code can't contain function
declaration;""", record['commentary'])

        self.assertEqual("bestPractice/Sbp_A1_T1.js", record['path'])
        self.assertEqual("Trying to declare function at the Block statement",
                         record['description'])
        self.assertEqual("", record['onlyStrict'])
        self.assertEqual("SyntaxError", record['negative'])
        self.assertEqual("http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls",
                         record['bestPractice'])

        self.assertEqual(""""use strict";
{
    function __func(){}
}

""", record['test'])

    @unittest.expectedFailure
    def test_nomatch(self):
        with self.assertRaisesRegexp(Exception, "unrecognized"):
            parseTestRecord("#!/usr/bin/env python", "random.py")

    def test_duplicate(self):
        with self.assertRaisesRegexp(Exception, "duplicate: foo"):
            parseTestRecord("""
// Copyright

/**
 * @foo bar
 * @foo bar
 */

1;
"""
                            , "name")

    def test_malformed(self):
        with self.assertRaisesRegexp(Exception, 'Malformed "@" attribute: name'):
            parseTestRecord("""
// Copyright

/**
 * @ baz
 * @foo bar
 */

1;
"""
                            , "name")

    def test_stripStars(self):
        self.assertEqual("", stripStars(""))
        self.assertEqual("foo", stripStars("\n* foo"))
        self.assertEqual("@foo bar", stripStars("\n* @foo bar"))
        self.assertEqual("@foo bar", stripStars("\n  *@foo bar"))


class TestYAMLParsing(unittest.TestCase):
    def test_test(self):
        self.assertTrue(True)

    def test_split(self):
        name = 'fixtures/test262-yaml-headers.js'
        contents = slurpFile(name)
        self.assertTrue('---' in contents)
        match = matchParts(contents, name)
        self.assertEqual("""---
info: >
    The production Block { } in strict code can't contain function
    declaration;
description: Trying to declare function at the Block statement
negative: SyntaxError
bestPractice: "http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls"
flags: [onlyStrict]
---""", match.group(2))

    def test_yamlParse(self):
        text = """
info: >
    The production Block { } in strict code can't contain function
    declaration;
description: Trying to declare function at the Block statement
negative: SyntaxError
bestPractice: "http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls"
flags: [onlyStrict]"""
        parsed = yaml.load(text)

        self.assertEqual("Trying to declare function at the Block statement",
                         parsed['description'])
        self.assertEqual("SyntaxError", parsed['negative'])
        self.assertEqual('http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls', parsed['bestPractice'])
        self.assertEqual(["onlyStrict"], parsed['flags'])
        self.assertEqual("The production Block { } in strict code can't contain function declaration;\n", parsed['info'])

    def test_hasYAML(self):
        self.assertTrue(hasYAML("---\n some: yaml\n\n---"))
        self.assertFalse(hasYAML("\n* Test description\n *\n * @foo bar\n* @noStrict\n"))

    def test_fixturehasYAML(self):
        name = 'fixtures/test262-yaml-headers.js'
        contents = slurpFile(name)
        self.assertTrue('---' in contents)
        match = matchParts(contents, name)
        self.assertTrue(hasYAML(match.group(2)))

    def test_missingKeys(self):
        result = {}
        yamlAttrParser(result, """---
    info: some info (note no flags or includes)
---""", "")
        self.assertEqual("some info (note no flags or includes)", result['commentary'])

    def test_overview(self):
        name = 'fixtures/test262-yaml-headers.js'
        contents = slurpFile(name)
        record = parseTestRecord(contents, name)

        self.assertEqual("""// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.""",
                         record['header'])
        self.assertEqual("The production Block { } in strict code can't contain function declaration;\n", record['commentary'])

        self.assertEqual("Trying to declare function at the Block statement",
                         record['description'])
        self.assertEqual(['onlyStrict'], record['flags'])
        self.assertEqual("", record['onlyStrict'])
        self.assertEqual("SyntaxError", record['negative'])
        self.assertEqual("http://wiki.ecmascript.org/doku.php?id=conventions:no_non_standard_strict_decls",
                         record['bestPractice'])

        self.assertEqual(""""use strict";
{
    function __func(){}
}

""", record['test'])

    def test_negative(self):
        name = 'fixtures/negative.js'
        contents = slurpFile(name)
        record = parseTestRecord(contents, name)

        self.assertEqual('early', record['negative']['phase'])
        self.assertEqual('SyntaxError', record['negative']['type'])

if __name__ == '__main__':
    unittest.main()
