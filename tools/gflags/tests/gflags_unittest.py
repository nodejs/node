#!/usr/bin/env python

# Copyright (c) 2007, Google Inc.
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

"Unittest for gflags.py module"

__pychecker__ = "no-local" # for unittest


import cStringIO
import sys
import os
import shutil

import gflags
from flags_modules_for_testing import module_foo
from flags_modules_for_testing import module_bar
from flags_modules_for_testing import module_baz

FLAGS=gflags.FLAGS

import gflags_googletest as googletest

# TODO(csilvers): add a wrapper function around FLAGS(argv) that
# verifies the input is a list or tuple.  This avoids bugs where we
# make argv a string instead of a list, by mistake.

class FlagsUnitTest(googletest.TestCase):
  "Flags Unit Test"

  def setUp(self):
    # make sure we are using the old, stupid way of parsing flags.
    FLAGS.UseGnuGetOpt(False)

  def test_flags(self):

    ##############################################
    # Test normal usage with no (expected) errors.

    # Define flags
    number_test_framework_flags = len(FLAGS.RegisteredFlags())
    repeatHelp = "how many times to repeat (0-5)"
    gflags.DEFINE_integer("repeat", 4, repeatHelp,
                         lower_bound=0, short_name='r')
    gflags.DEFINE_string("name", "Bob", "namehelp")
    gflags.DEFINE_boolean("debug", 0, "debughelp")
    gflags.DEFINE_boolean("q", 1, "quiet mode")
    gflags.DEFINE_boolean("quack", 0, "superstring of 'q'")
    gflags.DEFINE_boolean("noexec", 1, "boolean flag with no as prefix")
    gflags.DEFINE_integer("x", 3, "how eXtreme to be")
    gflags.DEFINE_integer("l", 0x7fffffff00000000, "how long to be")
    gflags.DEFINE_list('letters', 'a,b,c', "a list of letters")
    gflags.DEFINE_list('numbers', [1, 2, 3], "a list of numbers")
    gflags.DEFINE_enum("kwery", None, ['who', 'what', 'why', 'where', 'when'],
                      "?")

    # Specify number of flags defined above.  The short_name defined
    # for 'repeat' counts as an extra flag.
    number_defined_flags = 11 + 1
    self.assertEqual(len(FLAGS.RegisteredFlags()),
                         number_defined_flags + number_test_framework_flags)

    assert FLAGS.repeat == 4, "integer default values not set:" + FLAGS.repeat
    assert FLAGS.name == 'Bob', "default values not set:" + FLAGS.name
    assert FLAGS.debug == 0, "boolean default values not set:" + FLAGS.debug
    assert FLAGS.q == 1, "boolean default values not set:" + FLAGS.q
    assert FLAGS.x == 3, "integer default values not set:" + FLAGS.x
    assert FLAGS.l == 0x7fffffff00000000, ("integer default values not set:"
                                           + FLAGS.l)
    assert FLAGS.letters == ['a', 'b', 'c'], ("list default values not set:"
                                              + FLAGS.letters)
    assert FLAGS.numbers == [1, 2, 3], ("list default values not set:"
                                        + FLAGS.numbers)
    assert FLAGS.kwery is None, ("enum default None value not set:"
                                  + FLAGS.kwery)

    flag_values = FLAGS.FlagValuesDict()
    assert flag_values['repeat'] == 4
    assert flag_values['name'] == 'Bob'
    assert flag_values['debug'] == 0
    assert flag_values['r'] == 4       # short for repeat
    assert flag_values['q'] == 1
    assert flag_values['quack'] == 0
    assert flag_values['x'] == 3
    assert flag_values['l'] == 0x7fffffff00000000
    assert flag_values['letters'] == ['a', 'b', 'c']
    assert flag_values['numbers'] == [1, 2, 3]
    assert flag_values['kwery'] is None

    # Verify string form of defaults
    assert FLAGS['repeat'].default_as_str == "'4'"
    assert FLAGS['name'].default_as_str == "'Bob'"
    assert FLAGS['debug'].default_as_str == "'false'"
    assert FLAGS['q'].default_as_str == "'true'"
    assert FLAGS['quack'].default_as_str == "'false'"
    assert FLAGS['noexec'].default_as_str == "'true'"
    assert FLAGS['x'].default_as_str == "'3'"
    assert FLAGS['l'].default_as_str == "'9223372032559808512'"
    assert FLAGS['letters'].default_as_str == "'a,b,c'"
    assert FLAGS['numbers'].default_as_str == "'1,2,3'"

    # Verify that the iterator for flags yields all the keys
    keys = list(FLAGS)
    keys.sort()
    reg_flags = FLAGS.RegisteredFlags()
    reg_flags.sort()
    self.assertEqual(keys, reg_flags)

    # Parse flags
    # .. empty command line
    argv = ('./program',)
    argv = FLAGS(argv)
    assert len(argv) == 1, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"

    # .. non-empty command line
    argv = ('./program', '--debug', '--name=Bob', '-q', '--x=8')
    argv = FLAGS(argv)
    assert len(argv) == 1, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert FLAGS['debug'].present == 1
    FLAGS['debug'].present = 0 # Reset
    assert FLAGS['name'].present == 1
    FLAGS['name'].present = 0 # Reset
    assert FLAGS['q'].present == 1
    FLAGS['q'].present = 0 # Reset
    assert FLAGS['x'].present == 1
    FLAGS['x'].present = 0 # Reset

    # Flags list
    self.assertEqual(len(FLAGS.RegisteredFlags()),
                     number_defined_flags + number_test_framework_flags)
    assert 'name' in FLAGS.RegisteredFlags()
    assert 'debug' in FLAGS.RegisteredFlags()
    assert 'repeat' in FLAGS.RegisteredFlags()
    assert 'r' in FLAGS.RegisteredFlags()
    assert 'q' in FLAGS.RegisteredFlags()
    assert 'quack' in FLAGS.RegisteredFlags()
    assert 'x' in FLAGS.RegisteredFlags()
    assert 'l' in FLAGS.RegisteredFlags()
    assert 'letters' in FLAGS.RegisteredFlags()
    assert 'numbers' in FLAGS.RegisteredFlags()

    # has_key
    assert FLAGS.has_key('name')
    assert not FLAGS.has_key('name2')
    assert 'name' in FLAGS
    assert 'name2' not in FLAGS

    # try deleting a flag
    del FLAGS.r
    self.assertEqual(len(FLAGS.RegisteredFlags()),
                     number_defined_flags - 1 + number_test_framework_flags)
    assert not 'r' in FLAGS.RegisteredFlags()

    # .. command line with extra stuff
    argv = ('./program', '--debug', '--name=Bob', 'extra')
    argv = FLAGS(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"
    assert FLAGS['debug'].present == 1
    FLAGS['debug'].present = 0 # Reset
    assert FLAGS['name'].present == 1
    FLAGS['name'].present = 0 # Reset

    # Test reset
    argv = ('./program', '--debug')
    argv = FLAGS(argv)
    assert len(argv) == 1, "wrong number of arguments pulled"
    assert argv[0] == './program', "program name not preserved"
    assert FLAGS['debug'].present == 1
    assert FLAGS['debug'].value
    FLAGS.Reset()
    assert FLAGS['debug'].present == 0
    assert not FLAGS['debug'].value

    # Test that reset restores default value when default value is None.
    argv = ('./program', '--kwery=who')
    argv = FLAGS(argv)
    assert len(argv) == 1, "wrong number of arguments pulled"
    assert argv[0] == './program', "program name not preserved"
    assert FLAGS['kwery'].present == 1
    assert FLAGS['kwery'].value == 'who'
    FLAGS.Reset()
    assert FLAGS['kwery'].present == 0
    assert FLAGS['kwery'].value == None

    # Test integer argument passing
    argv = ('./program', '--x', '0x12345')
    argv = FLAGS(argv)
    self.assertEquals(FLAGS.x, 0x12345)
    self.assertEquals(type(FLAGS.x), int)

    argv = ('./program', '--x', '0x1234567890ABCDEF1234567890ABCDEF')
    argv = FLAGS(argv)
    self.assertEquals(FLAGS.x, 0x1234567890ABCDEF1234567890ABCDEF)
    self.assertEquals(type(FLAGS.x), long)

    # Treat 0-prefixed parameters as base-10, not base-8
    argv = ('./program', '--x', '012345')
    argv = FLAGS(argv)
    self.assertEquals(FLAGS.x, 12345)
    self.assertEquals(type(FLAGS.x), int)

    argv = ('./program', '--x', '0123459')
    argv = FLAGS(argv)
    self.assertEquals(FLAGS.x, 123459)
    self.assertEquals(type(FLAGS.x), int)

    argv = ('./program', '--x', '0x123efg')
    try:
      argv = FLAGS(argv)
      raise AssertionError("failed to detect invalid hex argument")
    except gflags.IllegalFlagValue:
      pass

    # Test boolean argument parsing
    gflags.DEFINE_boolean("test0", None, "test boolean parsing")
    argv = ('./program', '--notest0')
    argv = FLAGS(argv)
    assert FLAGS.test0 == 0

    gflags.DEFINE_boolean("test1", None, "test boolean parsing")
    argv = ('./program', '--test1')
    argv = FLAGS(argv)
    assert FLAGS.test1 == 1

    FLAGS.test0 = None
    argv = ('./program', '--test0=false')
    argv = FLAGS(argv)
    assert FLAGS.test0 == 0

    FLAGS.test1 = None
    argv = ('./program', '--test1=true')
    argv = FLAGS(argv)
    assert FLAGS.test1 == 1

    FLAGS.test0 = None
    argv = ('./program', '--test0=0')
    argv = FLAGS(argv)
    assert FLAGS.test0 == 0

    FLAGS.test1 = None
    argv = ('./program', '--test1=1')
    argv = FLAGS(argv)
    assert FLAGS.test1 == 1

    # Test booleans that already have 'no' as a prefix
    FLAGS.noexec = None
    argv = ('./program', '--nonoexec', '--name', 'Bob')
    argv = FLAGS(argv)
    assert FLAGS.noexec == 0

    FLAGS.noexec = None
    argv = ('./program', '--name', 'Bob', '--noexec')
    argv = FLAGS(argv)
    assert FLAGS.noexec == 1

    # Test unassigned booleans
    gflags.DEFINE_boolean("testnone", None, "test boolean parsing")
    argv = ('./program',)
    argv = FLAGS(argv)
    assert FLAGS.testnone == None

    # Test get with default
    gflags.DEFINE_boolean("testget1", None, "test parsing with defaults")
    gflags.DEFINE_boolean("testget2", None, "test parsing with defaults")
    gflags.DEFINE_boolean("testget3", None, "test parsing with defaults")
    gflags.DEFINE_integer("testget4", None, "test parsing with defaults")
    argv = ('./program','--testget1','--notestget2')
    argv = FLAGS(argv)
    assert FLAGS.get('testget1', 'foo') == 1
    assert FLAGS.get('testget2', 'foo') == 0
    assert FLAGS.get('testget3', 'foo') == 'foo'
    assert FLAGS.get('testget4', 'foo') == 'foo'

    # test list code
    lists = [['hello','moo','boo','1'],
             [],]

    gflags.DEFINE_list('testlist', '', 'test lists parsing')
    gflags.DEFINE_spaceseplist('testspacelist', '', 'tests space lists parsing')

    for name, sep in (('testlist', ','), ('testspacelist', ' '),
                      ('testspacelist', '\n')):
      for lst in lists:
        argv = ('./program', '--%s=%s' % (name, sep.join(lst)))
        argv = FLAGS(argv)
        self.assertEquals(getattr(FLAGS, name), lst)

    # Test help text
    flagsHelp = str(FLAGS)
    assert flagsHelp.find("repeat") != -1, "cannot find flag in help"
    assert flagsHelp.find(repeatHelp) != -1, "cannot find help string in help"

    # Test flag specified twice
    argv = ('./program', '--repeat=4', '--repeat=2', '--debug', '--nodebug')
    argv = FLAGS(argv)
    self.assertEqual(FLAGS.get('repeat', None), 2)
    self.assertEqual(FLAGS.get('debug', None), 0)

    # Test MultiFlag with single default value
    gflags.DEFINE_multistring('s_str', 'sing1',
                             'string option that can occur multiple times',
                             short_name='s')
    self.assertEqual(FLAGS.get('s_str', None), [ 'sing1', ])

    # Test MultiFlag with list of default values
    multi_string_defs = [ 'def1', 'def2', ]
    gflags.DEFINE_multistring('m_str', multi_string_defs,
                             'string option that can occur multiple times',
                             short_name='m')
    self.assertEqual(FLAGS.get('m_str', None), multi_string_defs)

    # Test flag specified multiple times with a MultiFlag
    argv = ('./program', '--m_str=str1', '-m', 'str2')
    argv = FLAGS(argv)
    self.assertEqual(FLAGS.get('m_str', None), [ 'str1', 'str2', ])

    # Test single-letter flags; should support both single and double dash
    argv = ('./program', '-q', '-x8')
    argv = FLAGS(argv)
    self.assertEqual(FLAGS.get('q', None), 1)
    self.assertEqual(FLAGS.get('x', None), 8)

    argv = ('./program', '--q', '--x', '9', '--noqu')
    argv = FLAGS(argv)
    self.assertEqual(FLAGS.get('q', None), 1)
    self.assertEqual(FLAGS.get('x', None), 9)
    # --noqu should match '--noquack since it's a unique prefix
    self.assertEqual(FLAGS.get('quack', None), 0)

    argv = ('./program', '--noq', '--x=10', '--qu')
    argv = FLAGS(argv)
    self.assertEqual(FLAGS.get('q', None), 0)
    self.assertEqual(FLAGS.get('x', None), 10)
    self.assertEqual(FLAGS.get('quack', None), 1)

    ####################################
    # Test flag serialization code:

    oldtestlist = FLAGS.testlist
    oldtestspacelist = FLAGS.testspacelist

    argv = ('./program',
            FLAGS['test0'].Serialize(),
            FLAGS['test1'].Serialize(),
            FLAGS['testnone'].Serialize(),
            FLAGS['s_str'].Serialize())
    argv = FLAGS(argv)
    self.assertEqual(FLAGS['test0'].Serialize(), '--notest0')
    self.assertEqual(FLAGS['test1'].Serialize(), '--test1')
    self.assertEqual(FLAGS['testnone'].Serialize(), '')
    self.assertEqual(FLAGS['s_str'].Serialize(), '--s_str=sing1')

    testlist1 = ['aa', 'bb']
    testspacelist1 = ['aa', 'bb', 'cc']
    FLAGS.testlist = list(testlist1)
    FLAGS.testspacelist = list(testspacelist1)
    argv = ('./program',
            FLAGS['testlist'].Serialize(),
            FLAGS['testspacelist'].Serialize())
    argv = FLAGS(argv)
    self.assertEqual(FLAGS.testlist, testlist1)
    self.assertEqual(FLAGS.testspacelist, testspacelist1)

    testlist1 = ['aa some spaces', 'bb']
    testspacelist1 = ['aa', 'bb,some,commas,', 'cc']
    FLAGS.testlist = list(testlist1)
    FLAGS.testspacelist = list(testspacelist1)
    argv = ('./program',
            FLAGS['testlist'].Serialize(),
            FLAGS['testspacelist'].Serialize())
    argv = FLAGS(argv)
    self.assertEqual(FLAGS.testlist, testlist1)
    self.assertEqual(FLAGS.testspacelist, testspacelist1)

    FLAGS.testlist = oldtestlist
    FLAGS.testspacelist = oldtestspacelist

    ####################################
    # Test flag-update:

    def ArgsString():
      flagnames = FLAGS.RegisteredFlags()

      flagnames.sort()
      nonbool_flags = ['--%s %s' % (name, FLAGS.get(name, None))
                       for name in flagnames
                       if not isinstance(FLAGS[name], gflags.BooleanFlag)]

      truebool_flags = ['--%s' % (name)
                        for name in flagnames
                        if isinstance(FLAGS[name], gflags.BooleanFlag) and
                          FLAGS.get(name, None)]
      falsebool_flags = ['--no%s' % (name)
                         for name in flagnames
                         if isinstance(FLAGS[name], gflags.BooleanFlag) and
                           not FLAGS.get(name, None)]
      return ' '.join(nonbool_flags + truebool_flags + falsebool_flags)

    argv = ('./program', '--repeat=3', '--name=giants', '--nodebug')

    FLAGS(argv)
    self.assertEqual(FLAGS.get('repeat', None), 3)
    self.assertEqual(FLAGS.get('name', None), 'giants')
    self.assertEqual(FLAGS.get('debug', None), 0)
    self.assertEqual(ArgsString(),
      "--kwery None "
      "--l 9223372032559808512 "
      "--letters ['a', 'b', 'c'] "
      "--m ['str1', 'str2'] --m_str ['str1', 'str2'] "
      "--name giants "
      "--numbers [1, 2, 3] "
      "--repeat 3 "
      "--s ['sing1'] --s_str ['sing1'] "
      ""
      ""
      "--testget4 None --testlist [] "
      "--testspacelist [] --x 10 "
      "--noexec --quack "
      "--test1 "
      "--testget1 --tmod_baz_x "
      "--no? --nodebug --nohelp --nohelpshort --nohelpxml --noq "
      ""
      "--notest0 --notestget2 --notestget3 --notestnone")

    argv = ('./program', '--debug', '--m_str=upd1', '-s', 'upd2')
    FLAGS(argv)
    self.assertEqual(FLAGS.get('repeat', None), 3)
    self.assertEqual(FLAGS.get('name', None), 'giants')
    self.assertEqual(FLAGS.get('debug', None), 1)

    # items appended to existing non-default value lists for --m/--m_str
    # new value overwrites default value (not appended to it) for --s/--s_str
    self.assertEqual(ArgsString(),
      "--kwery None "
      "--l 9223372032559808512 "
      "--letters ['a', 'b', 'c'] "
      "--m ['str1', 'str2', 'upd1'] "
      "--m_str ['str1', 'str2', 'upd1'] "
      "--name giants "
      "--numbers [1, 2, 3] "
      "--repeat 3 "
      "--s ['upd2'] --s_str ['upd2'] "
      ""
      ""
      "--testget4 None --testlist [] "
      "--testspacelist [] --x 10 "
      "--debug --noexec --quack "
      "--test1 "
      "--testget1 --tmod_baz_x "
      "--no? --nohelp --nohelpshort --nohelpxml --noq "
      ""
      "--notest0 --notestget2 --notestget3 --notestnone")

    ####################################
    # Test all kind of error conditions.

    # Duplicate flag detection
    try:
      gflags.DEFINE_boolean("run", 0, "runhelp", short_name='q')
      raise AssertionError("duplicate flag detection failed")
    except gflags.DuplicateFlag:
      pass

    # Duplicate short flag detection
    try:
      gflags.DEFINE_boolean("zoom1", 0, "runhelp z1", short_name='z')
      gflags.DEFINE_boolean("zoom2", 0, "runhelp z2", short_name='z')
      raise AssertionError("duplicate short flag detection failed")
    except gflags.DuplicateFlag, e:
      self.assertTrue("The flag 'z' is defined twice. " in e.args[0])
      self.assertTrue("First from" in e.args[0])
      self.assertTrue(", Second from" in e.args[0])

    # Duplicate mixed flag detection
    try:
      gflags.DEFINE_boolean("short1", 0, "runhelp s1", short_name='s')
      gflags.DEFINE_boolean("s", 0, "runhelp s2")
      raise AssertionError("duplicate mixed flag detection failed")
    except gflags.DuplicateFlag, e:
      self.assertTrue("The flag 's' is defined twice. " in e.args[0])
      self.assertTrue("First from" in e.args[0])
      self.assertTrue(", Second from" in e.args[0])

    # Check that duplicate flag detection detects definition sites
    # correctly.
    flagnames = ["repeated"]
    original_flags = gflags.FlagValues()
    gflags.DEFINE_boolean(flagnames[0], False, "Flag about to be repeated.",
                         flag_values=original_flags)
    duplicate_flags = module_foo.DuplicateFlags(flagnames)
    try:
      original_flags.AppendFlagValues(duplicate_flags)
    except gflags.DuplicateFlagError, e:
      self.assertTrue("flags_unittest" in str(e))
      self.assertTrue("module_foo" in str(e))

    # Make sure allow_override works
    try:
      gflags.DEFINE_boolean("dup1", 0, "runhelp d11", short_name='u',
                           allow_override=0)
      flag = FLAGS.FlagDict()['dup1']
      self.assertEqual(flag.default, 0)

      gflags.DEFINE_boolean("dup1", 1, "runhelp d12", short_name='u',
                           allow_override=1)
      flag = FLAGS.FlagDict()['dup1']
      self.assertEqual(flag.default, 1)
    except gflags.DuplicateFlag:
      raise AssertionError("allow_override did not permit a flag duplication")

    # Make sure allow_override works
    try:
      gflags.DEFINE_boolean("dup2", 0, "runhelp d21", short_name='u',
                           allow_override=1)
      flag = FLAGS.FlagDict()['dup2']
      self.assertEqual(flag.default, 0)

      gflags.DEFINE_boolean("dup2", 1, "runhelp d22", short_name='u',
                           allow_override=0)
      flag = FLAGS.FlagDict()['dup2']
      self.assertEqual(flag.default, 1)
    except gflags.DuplicateFlag:
      raise AssertionError("allow_override did not permit a flag duplication")

    # Make sure allow_override doesn't work with None default
    try:
      gflags.DEFINE_boolean("dup3", 0, "runhelp d31", short_name='u3',
                           allow_override=0)
      flag = FLAGS.FlagDict()['dup3']
      self.assertEqual(flag.default, 0)

      gflags.DEFINE_boolean("dup3", None, "runhelp d32", short_name='u3',
                           allow_override=1)
      raise AssertionError('Cannot override a flag with a default of None')
    except gflags.DuplicateFlagCannotPropagateNoneToSwig:
      pass

    # Make sure that re-importing a module does not cause a DuplicateFlagError
    # to be raised.
    try:
      sys.modules.pop(
          "flags_modules_for_testing.module_baz")
      import flags_modules_for_testing.module_baz
    except gflags.DuplicateFlagError:
      raise AssertionError("Module reimport caused flag duplication error")

    # Make sure that when we override, the help string gets updated correctly
    gflags.DEFINE_boolean("dup3", 0, "runhelp d31", short_name='u',
                         allow_override=1)
    gflags.DEFINE_boolean("dup3", 1, "runhelp d32", short_name='u',
                         allow_override=1)
    self.assert_(str(FLAGS).find('runhelp d31') == -1)
    self.assert_(str(FLAGS).find('runhelp d32') != -1)

    # Make sure AppendFlagValues works
    new_flags = gflags.FlagValues()
    gflags.DEFINE_boolean("new1", 0, "runhelp n1", flag_values=new_flags)
    gflags.DEFINE_boolean("new2", 0, "runhelp n2", flag_values=new_flags)
    self.assertEqual(len(new_flags.FlagDict()), 2)
    old_len = len(FLAGS.FlagDict())
    FLAGS.AppendFlagValues(new_flags)
    self.assertEqual(len(FLAGS.FlagDict())-old_len, 2)
    self.assertEqual("new1" in FLAGS.FlagDict(), True)
    self.assertEqual("new2" in FLAGS.FlagDict(), True)

    # Then test that removing those flags works
    FLAGS.RemoveFlagValues(new_flags)
    self.assertEqual(len(FLAGS.FlagDict()), old_len)
    self.assertFalse("new1" in FLAGS.FlagDict())
    self.assertFalse("new2" in FLAGS.FlagDict())

    # Make sure AppendFlagValues works with flags with shortnames.
    new_flags = gflags.FlagValues()
    gflags.DEFINE_boolean("new3", 0, "runhelp n3", flag_values=new_flags)
    gflags.DEFINE_boolean("new4", 0, "runhelp n4", flag_values=new_flags,
                         short_name="n4")
    self.assertEqual(len(new_flags.FlagDict()), 3)
    old_len = len(FLAGS.FlagDict())
    FLAGS.AppendFlagValues(new_flags)
    self.assertEqual(len(FLAGS.FlagDict())-old_len, 3)
    self.assertTrue("new3" in FLAGS.FlagDict())
    self.assertTrue("new4" in FLAGS.FlagDict())
    self.assertTrue("n4" in FLAGS.FlagDict())
    self.assertEqual(FLAGS.FlagDict()['n4'], FLAGS.FlagDict()['new4'])

    # Then test removing them
    FLAGS.RemoveFlagValues(new_flags)
    self.assertEqual(len(FLAGS.FlagDict()), old_len)
    self.assertFalse("new3" in FLAGS.FlagDict())
    self.assertFalse("new4" in FLAGS.FlagDict())
    self.assertFalse("n4" in FLAGS.FlagDict())

    # Make sure AppendFlagValues fails on duplicates
    gflags.DEFINE_boolean("dup4", 0, "runhelp d41")
    new_flags = gflags.FlagValues()
    gflags.DEFINE_boolean("dup4", 0, "runhelp d42", flag_values=new_flags)
    try:
      FLAGS.AppendFlagValues(new_flags)
      raise AssertionError("ignore_copy was not set but caused no exception")
    except gflags.DuplicateFlag:
      pass

    # Integer out of bounds
    try:
      argv = ('./program', '--repeat=-4')
      FLAGS(argv)
      raise AssertionError('integer bounds exception not raised:'
                           + str(FLAGS.repeat))
    except gflags.IllegalFlagValue:
      pass

    # Non-integer
    try:
      argv = ('./program', '--repeat=2.5')
      FLAGS(argv)
      raise AssertionError("malformed integer value exception not raised")
    except gflags.IllegalFlagValue:
      pass

    # Missing required arugment
    try:
      argv = ('./program', '--name')
      FLAGS(argv)
      raise AssertionError("Flag argument required exception not raised")
    except gflags.FlagsError:
      pass

    # Non-boolean arguments for boolean
    try:
      argv = ('./program', '--debug=goofup')
      FLAGS(argv)
      raise AssertionError("Illegal flag value exception not raised")
    except gflags.IllegalFlagValue:
      pass

    try:
      argv = ('./program', '--debug=42')
      FLAGS(argv)
      raise AssertionError("Illegal flag value exception not raised")
    except gflags.IllegalFlagValue:
      pass


    # Non-numeric argument for integer flag --repeat
    try:
      argv = ('./program', '--repeat', 'Bob', 'extra')
      FLAGS(argv)
      raise AssertionError("Illegal flag value exception not raised")
    except gflags.IllegalFlagValue:
      pass

    # Test ModuleHelp().
    helpstr = FLAGS.ModuleHelp(module_baz)

    expected_help = "\n" + module_baz.__name__ + ":" + """
  --[no]tmod_baz_x: Boolean flag.
    (default: 'true')"""

    self.assertMultiLineEqual(expected_help, helpstr)

    # Test MainModuleHelp().  This must be part of test_flags because
    # it dpeends on dup1/2/3/etc being introduced first.
    helpstr = FLAGS.MainModuleHelp()

    expected_help = "\n" + sys.argv[0] + ':' + """
  --[no]debug: debughelp
    (default: 'false')
  -u,--[no]dup1: runhelp d12
    (default: 'true')
  -u,--[no]dup2: runhelp d22
    (default: 'true')
  -u,--[no]dup3: runhelp d32
    (default: 'true')
  --[no]dup4: runhelp d41
    (default: 'false')
  --kwery: <who|what|why|where|when>: ?
  --l: how long to be
    (default: '9223372032559808512')
    (an integer)
  --letters: a list of letters
    (default: 'a,b,c')
    (a comma separated list)
  -m,--m_str: string option that can occur multiple times;
    repeat this option to specify a list of values
    (default: "['def1', 'def2']")
  --name: namehelp
    (default: 'Bob')
  --[no]noexec: boolean flag with no as prefix
    (default: 'true')
  --numbers: a list of numbers
    (default: '1,2,3')
    (a comma separated list)
  --[no]q: quiet mode
    (default: 'true')
  --[no]quack: superstring of 'q'
    (default: 'false')
  -r,--repeat: how many times to repeat (0-5)
    (default: '4')
    (a non-negative integer)
  -s,--s_str: string option that can occur multiple times;
    repeat this option to specify a list of values
    (default: "['sing1']")
  --[no]test0: test boolean parsing
  --[no]test1: test boolean parsing
  --[no]testget1: test parsing with defaults
  --[no]testget2: test parsing with defaults
  --[no]testget3: test parsing with defaults
  --testget4: test parsing with defaults
    (an integer)
  --testlist: test lists parsing
    (default: '')
    (a comma separated list)
  --[no]testnone: test boolean parsing
  --testspacelist: tests space lists parsing
    (default: '')
    (a whitespace separated list)
  --x: how eXtreme to be
    (default: '3')
    (an integer)
  -z,--[no]zoom1: runhelp z1
    (default: 'false')"""

    # Insert the --help flags in their proper place.
    help_help = """\
  -?,--[no]help: show this help
  --[no]helpshort: show usage only for this module
  --[no]helpxml: like --help, but generates XML output
"""
    expected_help = expected_help.replace('  --kwery',
                                          help_help + '  --kwery')

    self.assertMultiLineEqual(expected_help, helpstr)


class MultiNumericalFlagsTest(googletest.TestCase):

  def testMultiNumericalFlags(self):
    """Test multi_int and multi_float flags."""

    int_defaults = [77, 88,]
    gflags.DEFINE_multi_int('m_int', int_defaults,
                           'integer option that can occur multiple times',
                           short_name='mi')
    self.assertListEqual(FLAGS.get('m_int', None), int_defaults)
    argv = ('./program', '--m_int=-99', '--mi=101')
    FLAGS(argv)
    self.assertListEqual(FLAGS.get('m_int', None), [-99, 101,])

    float_defaults = [2.2, 3]
    gflags.DEFINE_multi_float('m_float', float_defaults,
                             'float option that can occur multiple times',
                             short_name='mf')
    for (expected, actual) in zip(float_defaults, FLAGS.get('m_float', None)):
      self.assertAlmostEquals(expected, actual)
    argv = ('./program', '--m_float=-17', '--mf=2.78e9')
    FLAGS(argv)
    expected_floats = [-17.0, 2.78e9]
    for (expected, actual) in zip(expected_floats, FLAGS.get('m_float', None)):
      self.assertAlmostEquals(expected, actual)

  def testSingleValueDefault(self):
    """Test multi_int and multi_float flags with a single default value."""
    int_default = 77
    gflags.DEFINE_multi_int('m_int1', int_default,
                           'integer option that can occur multiple times')
    self.assertListEqual(FLAGS.get('m_int1', None), [int_default])

    float_default = 2.2
    gflags.DEFINE_multi_float('m_float1', float_default,
                             'float option that can occur multiple times')
    actual = FLAGS.get('m_float1', None)
    self.assertEquals(1, len(actual))
    self.assertAlmostEquals(actual[0], float_default)

  def testBadMultiNumericalFlags(self):
    """Test multi_int and multi_float flags with non-parseable values."""

    # Test non-parseable defaults.
    self.assertRaisesWithRegexpMatch(
        gflags.IllegalFlagValue,
        'flag --m_int2=abc: invalid literal for int\(\) with base 10: \'abc\'',
        gflags.DEFINE_multi_int, 'm_int2', ['abc'], 'desc')

    self.assertRaisesWithRegexpMatch(
        gflags.IllegalFlagValue,
        'flag --m_float2=abc: invalid literal for float\(\): abc',
        gflags.DEFINE_multi_float, 'm_float2', ['abc'], 'desc')

    # Test non-parseable command line values.
    gflags.DEFINE_multi_int('m_int2', '77',
                           'integer option that can occur multiple times')
    argv = ('./program', '--m_int2=def')
    self.assertRaisesWithRegexpMatch(
        gflags.IllegalFlagValue,
        'flag --m_int2=def: invalid literal for int\(\) with base 10: \'def\'',
        FLAGS, argv)

    gflags.DEFINE_multi_float('m_float2', 2.2,
                             'float option that can occur multiple times')
    argv = ('./program', '--m_float2=def')
    self.assertRaisesWithRegexpMatch(
        gflags.IllegalFlagValue,
        'flag --m_float2=def: invalid literal for float\(\): def',
        FLAGS, argv)


class UnicodeFlagsTest(googletest.TestCase):
  """Testing proper unicode support for flags."""

  def testUnicodeDefaultAndHelpstring(self):
    gflags.DEFINE_string("unicode_str", "\xC3\x80\xC3\xBD".decode("utf-8"),
                        "help:\xC3\xAA".decode("utf-8"))
    argv = ("./program",)
    FLAGS(argv)   # should not raise any exceptions

    argv = ("./program", "--unicode_str=foo")
    FLAGS(argv)   # should not raise any exceptions

  def testUnicodeInList(self):
    gflags.DEFINE_list("unicode_list", ["abc", "\xC3\x80".decode("utf-8"),
                                       "\xC3\xBD".decode("utf-8")],
                      "help:\xC3\xAB".decode("utf-8"))
    argv = ("./program",)
    FLAGS(argv)   # should not raise any exceptions

    argv = ("./program", "--unicode_list=hello,there")
    FLAGS(argv)   # should not raise any exceptions

  def testXMLOutput(self):
    gflags.DEFINE_string("unicode1", "\xC3\x80\xC3\xBD".decode("utf-8"),
                        "help:\xC3\xAC".decode("utf-8"))
    gflags.DEFINE_list("unicode2", ["abc", "\xC3\x80".decode("utf-8"),
                                   "\xC3\xBD".decode("utf-8")],
                      "help:\xC3\xAD".decode("utf-8"))
    gflags.DEFINE_list("non_unicode", ["abc", "def", "ghi"],
                      "help:\xC3\xAD".decode("utf-8"))

    outfile = cStringIO.StringIO()
    FLAGS.WriteHelpInXMLFormat(outfile)
    actual_output = outfile.getvalue()

    # The xml output is large, so we just check parts of it.
    self.assertTrue("<name>unicode1</name>\n"
                    "    <meaning>help:&#236;</meaning>\n"
                    "    <default>&#192;&#253;</default>\n"
                    "    <current>&#192;&#253;</current>"
                    in actual_output)
    self.assertTrue("<name>unicode2</name>\n"
                    "    <meaning>help:&#237;</meaning>\n"
                    "    <default>abc,&#192;,&#253;</default>\n"
                    "    <current>[\'abc\', u\'\\xc0\', u\'\\xfd\']</current>"
                    in actual_output)
    self.assertTrue("<name>non_unicode</name>\n"
                    "    <meaning>help:&#237;</meaning>\n"
                    "    <default>abc,def,ghi</default>\n"
                    "    <current>[\'abc\', \'def\', \'ghi\']</current>"
                    in actual_output)


class LoadFromFlagFileTest(googletest.TestCase):
  """Testing loading flags from a file and parsing them."""

  def setUp(self):
    self.flag_values = gflags.FlagValues()
    # make sure we are using the old, stupid way of parsing flags.
    self.flag_values.UseGnuGetOpt(False)
    gflags.DEFINE_string('UnitTestMessage1', 'Foo!', 'You Add Here.',
                        flag_values=self.flag_values)
    gflags.DEFINE_string('UnitTestMessage2', 'Bar!', 'Hello, Sailor!',
                        flag_values=self.flag_values)
    gflags.DEFINE_boolean('UnitTestBoolFlag', 0, 'Some Boolean thing',
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('UnitTestNumber', 12345, 'Some integer',
                         lower_bound=0, flag_values=self.flag_values)
    gflags.DEFINE_list('UnitTestList', "1,2,3", 'Some list',
                      flag_values=self.flag_values)
    self.files_to_delete = []

  def tearDown(self):
    self._RemoveTestFiles()

  def _SetupTestFiles(self):
    """ Creates and sets up some dummy flagfile files with bogus flags"""

    # Figure out where to create temporary files
    tmp_path = '/tmp/flags_unittest'
    if os.path.exists(tmp_path):
      shutil.rmtree(tmp_path)
    os.makedirs(tmp_path)

    try:
      tmp_flag_file_1 = open(tmp_path + '/UnitTestFile1.tst', 'w')
      tmp_flag_file_2 = open(tmp_path + '/UnitTestFile2.tst', 'w')
      tmp_flag_file_3 = open(tmp_path + '/UnitTestFile3.tst', 'w')
      tmp_flag_file_4 = open(tmp_path + '/UnitTestFile4.tst', 'w')
    except IOError, e_msg:
      print e_msg
      print 'FAIL\n File Creation problem in Unit Test'
      sys.exit(1)

    # put some dummy flags in our test files
    tmp_flag_file_1.write('#A Fake Comment\n')
    tmp_flag_file_1.write('--UnitTestMessage1=tempFile1!\n')
    tmp_flag_file_1.write('\n')
    tmp_flag_file_1.write('--UnitTestNumber=54321\n')
    tmp_flag_file_1.write('--noUnitTestBoolFlag\n')
    file_list = [tmp_flag_file_1.name]
    # this one includes test file 1
    tmp_flag_file_2.write('//A Different Fake Comment\n')
    tmp_flag_file_2.write('--flagfile=%s\n' % tmp_flag_file_1.name)
    tmp_flag_file_2.write('--UnitTestMessage2=setFromTempFile2\n')
    tmp_flag_file_2.write('\t\t\n')
    tmp_flag_file_2.write('--UnitTestNumber=6789a\n')
    file_list.append(tmp_flag_file_2.name)
    # this file points to itself
    tmp_flag_file_3.write('--flagfile=%s\n' % tmp_flag_file_3.name)
    tmp_flag_file_3.write('--UnitTestMessage1=setFromTempFile3\n')
    tmp_flag_file_3.write('#YAFC\n')
    tmp_flag_file_3.write('--UnitTestBoolFlag\n')
    file_list.append(tmp_flag_file_3.name)
    # this file is unreadable
    tmp_flag_file_4.write('--flagfile=%s\n' % tmp_flag_file_3.name)
    tmp_flag_file_4.write('--UnitTestMessage1=setFromTempFile3\n')
    tmp_flag_file_4.write('--UnitTestMessage1=setFromTempFile3\n')
    os.chmod(tmp_path + '/UnitTestFile4.tst', 0)
    file_list.append(tmp_flag_file_4.name)

    tmp_flag_file_1.close()
    tmp_flag_file_2.close()
    tmp_flag_file_3.close()
    tmp_flag_file_4.close()

    self.files_to_delete = file_list

    return file_list # these are just the file names
  # end SetupFiles def

  def _RemoveTestFiles(self):
    """Closes the files we just created.  tempfile deletes them for us """
    for file_name in self.files_to_delete:
      try:
        os.remove(file_name)
      except OSError, e_msg:
        print '%s\n, Problem deleting test file' % e_msg
  #end RemoveTestFiles def

  def _ReadFlagsFromFiles(self, argv, force_gnu):
    return argv[:1] + self.flag_values.ReadFlagsFromFiles(argv[1:],
                                                          force_gnu=force_gnu)

  #### Flagfile Unit Tests ####
  def testMethod_flagfiles_1(self):
    """ Test trivial case with no flagfile based options. """
    fake_cmd_line = 'fooScript --UnitTestBoolFlag'
    fake_argv = fake_cmd_line.split(' ')
    self.flag_values(fake_argv)
    self.assertEqual( self.flag_values.UnitTestBoolFlag, 1)
    self.assertEqual( fake_argv, self._ReadFlagsFromFiles(fake_argv, False))

  # end testMethodOne

  def testMethod_flagfiles_2(self):
    """Tests parsing one file + arguments off simulated argv"""
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = 'fooScript --q --flagfile=%s' % tmp_files[0]
    fake_argv = fake_cmd_line.split(' ')

    # We should see the original cmd line with the file's contents spliced in.
    # Flags from the file will appear in the order order they are sepcified
    # in the file, in the same position as the flagfile argument.
    expected_results = ['fooScript',
                          '--q',
                          '--UnitTestMessage1=tempFile1!',
                          '--UnitTestNumber=54321',
                          '--noUnitTestBoolFlag']
    test_results = self._ReadFlagsFromFiles(fake_argv, False)
    self.assertEqual(expected_results, test_results)
  # end testTwo def

  def testMethod_flagfiles_3(self):
    """Tests parsing nested files + arguments of simulated argv"""
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = ('fooScript --UnitTestNumber=77 --flagfile=%s'
                     % tmp_files[1])
    fake_argv = fake_cmd_line.split(' ')

    expected_results = ['fooScript',
                          '--UnitTestNumber=77',
                          '--UnitTestMessage1=tempFile1!',
                          '--UnitTestNumber=54321',
                          '--noUnitTestBoolFlag',
                          '--UnitTestMessage2=setFromTempFile2',
                          '--UnitTestNumber=6789a']
    test_results = self._ReadFlagsFromFiles(fake_argv, False)
    self.assertEqual(expected_results, test_results)
  # end testThree def

  def testMethod_flagfiles_4(self):
    """Tests parsing self-referential files + arguments of simulated argv.
      This test should print a warning to stderr of some sort.
    """
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = ('fooScript --flagfile=%s --noUnitTestBoolFlag'
                     % tmp_files[2])
    fake_argv = fake_cmd_line.split(' ')
    expected_results = ['fooScript',
                          '--UnitTestMessage1=setFromTempFile3',
                          '--UnitTestBoolFlag',
                          '--noUnitTestBoolFlag' ]

    test_results = self._ReadFlagsFromFiles(fake_argv, False)
    self.assertEqual(expected_results, test_results)

  def testMethod_flagfiles_5(self):
    """Test that --flagfile parsing respects the '--' end-of-options marker."""
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = 'fooScript --SomeFlag -- --flagfile=%s' % tmp_files[0]
    fake_argv = fake_cmd_line.split(' ')
    expected_results = ['fooScript',
                        '--SomeFlag',
                        '--',
                        '--flagfile=%s' % tmp_files[0]]

    test_results = self._ReadFlagsFromFiles(fake_argv, False)
    self.assertEqual(expected_results, test_results)

  def testMethod_flagfiles_6(self):
    """Test that --flagfile parsing stops at non-options (non-GNU behavior)."""
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = ('fooScript --SomeFlag some_arg --flagfile=%s'
                     % tmp_files[0])
    fake_argv = fake_cmd_line.split(' ')
    expected_results = ['fooScript',
                        '--SomeFlag',
                        'some_arg',
                        '--flagfile=%s' % tmp_files[0]]

    test_results = self._ReadFlagsFromFiles(fake_argv, False)
    self.assertEqual(expected_results, test_results)

  def testMethod_flagfiles_7(self):
    """Test that --flagfile parsing skips over a non-option (GNU behavior)."""
    self.flag_values.UseGnuGetOpt()
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = ('fooScript --SomeFlag some_arg --flagfile=%s'
                     % tmp_files[0])
    fake_argv = fake_cmd_line.split(' ')
    expected_results = ['fooScript',
                        '--SomeFlag',
                        'some_arg',
                        '--UnitTestMessage1=tempFile1!',
                        '--UnitTestNumber=54321',
                        '--noUnitTestBoolFlag']

    test_results = self._ReadFlagsFromFiles(fake_argv, False)
    self.assertEqual(expected_results, test_results)

  def testMethod_flagfiles_8(self):
    """Test that --flagfile parsing respects force_gnu=True."""
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = ('fooScript --SomeFlag some_arg --flagfile=%s'
                     % tmp_files[0])
    fake_argv = fake_cmd_line.split(' ')
    expected_results = ['fooScript',
                        '--SomeFlag',
                        'some_arg',
                        '--UnitTestMessage1=tempFile1!',
                        '--UnitTestNumber=54321',
                        '--noUnitTestBoolFlag']

    test_results = self._ReadFlagsFromFiles(fake_argv, True)
    self.assertEqual(expected_results, test_results)

  def testMethod_flagfiles_NoPermissions(self):
    """Test that --flagfile raises except on file that is unreadable."""
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = ('fooScript --SomeFlag some_arg --flagfile=%s'
                     % tmp_files[3])
    fake_argv = fake_cmd_line.split(' ')
    self.assertRaises(gflags.CantOpenFlagFileError,
                      self._ReadFlagsFromFiles, fake_argv, True)

  def testMethod_flagfiles_NotFound(self):
    """Test that --flagfile raises except on file that does not exist."""
    tmp_files = self._SetupTestFiles()
    # specify our temp file on the fake cmd line
    fake_cmd_line = ('fooScript --SomeFlag some_arg --flagfile=%sNOTEXIST'
                     % tmp_files[3])
    fake_argv = fake_cmd_line.split(' ')
    self.assertRaises(gflags.CantOpenFlagFileError,
                      self._ReadFlagsFromFiles, fake_argv, True)

  def test_flagfiles_user_path_expansion(self):
    """Test that user directory referenced paths (ie. ~/foo) are correctly
      expanded.  This test depends on whatever account's running the unit test
      to have read/write access to their own home directory, otherwise it'll
      FAIL.
    """
    fake_flagfile_item_style_1 = '--flagfile=~/foo.file'
    fake_flagfile_item_style_2 = '-flagfile=~/foo.file'

    expected_results = os.path.expanduser('~/foo.file')

    test_results = self.flag_values.ExtractFilename(fake_flagfile_item_style_1)
    self.assertEqual(expected_results, test_results)

    test_results = self.flag_values.ExtractFilename(fake_flagfile_item_style_2)
    self.assertEqual(expected_results, test_results)

  # end testFour def

  def test_no_touchy_non_flags(self):
    """
    Test that the flags parser does not mutilate arguments which are
    not supposed to be flags
    """
    fake_argv = ['fooScript', '--UnitTestBoolFlag',
                 'command', '--command_arg1', '--UnitTestBoom', '--UnitTestB']
    argv = self.flag_values(fake_argv)
    self.assertEqual(argv, fake_argv[:1] + fake_argv[2:])

  def test_parse_flags_after_args_if_using_gnu_getopt(self):
    """
    Test that flags given after arguments are parsed if using gnu_getopt.
    """
    self.flag_values.UseGnuGetOpt()
    fake_argv = ['fooScript', '--UnitTestBoolFlag',
                 'command', '--UnitTestB']
    argv = self.flag_values(fake_argv)
    self.assertEqual(argv, ['fooScript', 'command'])

  def test_SetDefault(self):
    """
    Test changing flag defaults.
    """
    # Test that SetDefault changes both the default and the value,
    # and that the value is changed when one is given as an option.
    self.flag_values['UnitTestMessage1'].SetDefault('New value')
    self.assertEqual(self.flag_values.UnitTestMessage1, 'New value')
    self.assertEqual(self.flag_values['UnitTestMessage1'].default_as_str,
                     "'New value'")
    self.flag_values([ 'dummyscript', '--UnitTestMessage1=Newer value' ])
    self.assertEqual(self.flag_values.UnitTestMessage1, 'Newer value')

    # Test that setting the default to None works correctly.
    self.flag_values['UnitTestNumber'].SetDefault(None)
    self.assertEqual(self.flag_values.UnitTestNumber, None)
    self.assertEqual(self.flag_values['UnitTestNumber'].default_as_str, None)
    self.flag_values([ 'dummyscript', '--UnitTestNumber=56' ])
    self.assertEqual(self.flag_values.UnitTestNumber, 56)

    # Test that setting the default to zero works correctly.
    self.flag_values['UnitTestNumber'].SetDefault(0)
    self.assertEqual(self.flag_values.UnitTestNumber, 0)
    self.assertEqual(self.flag_values['UnitTestNumber'].default_as_str, "'0'")
    self.flag_values([ 'dummyscript', '--UnitTestNumber=56' ])
    self.assertEqual(self.flag_values.UnitTestNumber, 56)

    # Test that setting the default to "" works correctly.
    self.flag_values['UnitTestMessage1'].SetDefault("")
    self.assertEqual(self.flag_values.UnitTestMessage1, "")
    self.assertEqual(self.flag_values['UnitTestMessage1'].default_as_str, "''")
    self.flag_values([ 'dummyscript', '--UnitTestMessage1=fifty-six' ])
    self.assertEqual(self.flag_values.UnitTestMessage1, "fifty-six")

    # Test that setting the default to false works correctly.
    self.flag_values['UnitTestBoolFlag'].SetDefault(False)
    self.assertEqual(self.flag_values.UnitTestBoolFlag, False)
    self.assertEqual(self.flag_values['UnitTestBoolFlag'].default_as_str,
                     "'false'")
    self.flag_values([ 'dummyscript', '--UnitTestBoolFlag=true' ])
    self.assertEqual(self.flag_values.UnitTestBoolFlag, True)

    # Test that setting a list default works correctly.
    self.flag_values['UnitTestList'].SetDefault('4,5,6')
    self.assertEqual(self.flag_values.UnitTestList, ['4', '5', '6'])
    self.assertEqual(self.flag_values['UnitTestList'].default_as_str, "'4,5,6'")
    self.flag_values([ 'dummyscript', '--UnitTestList=7,8,9' ])
    self.assertEqual(self.flag_values.UnitTestList, ['7', '8', '9'])

    # Test that setting invalid defaults raises exceptions
    self.assertRaises(gflags.IllegalFlagValue,
                      self.flag_values['UnitTestNumber'].SetDefault, 'oops')
    self.assertRaises(gflags.IllegalFlagValue,
                      self.flag_values.SetDefault, 'UnitTestNumber', -1)


class FlagsParsingTest(googletest.TestCase):
  """Testing different aspects of parsing: '-f' vs '--flag', etc."""

  def setUp(self):
    self.flag_values = gflags.FlagValues()

  def testMethod_ShortestUniquePrefixes(self):
    """Test FlagValues.ShortestUniquePrefixes"""

    gflags.DEFINE_string('a', '', '', flag_values=self.flag_values)
    gflags.DEFINE_string('abc', '', '', flag_values=self.flag_values)
    gflags.DEFINE_string('common_a_string', '', '', flag_values=self.flag_values)
    gflags.DEFINE_boolean('common_b_boolean', 0, '',
                         flag_values=self.flag_values)
    gflags.DEFINE_boolean('common_c_boolean', 0, '',
                         flag_values=self.flag_values)
    gflags.DEFINE_boolean('common', 0, '', flag_values=self.flag_values)
    gflags.DEFINE_integer('commonly', 0, '', flag_values=self.flag_values)
    gflags.DEFINE_boolean('zz', 0, '', flag_values=self.flag_values)
    gflags.DEFINE_integer('nozz', 0, '', flag_values=self.flag_values)

    shorter_flags = self.flag_values.ShortestUniquePrefixes(
        self.flag_values.FlagDict())

    expected_results = {'nocommon_b_boolean': 'nocommon_b',
                        'common_c_boolean': 'common_c',
                        'common_b_boolean': 'common_b',
                        'a': 'a',
                        'abc': 'ab',
                        'zz': 'z',
                        'nozz': 'nozz',
                        'common_a_string': 'common_a',
                        'commonly': 'commonl',
                        'nocommon_c_boolean': 'nocommon_c',
                        'nocommon': 'nocommon',
                        'common': 'common'}

    for name, shorter in expected_results.iteritems():
      self.assertEquals(shorter_flags[name], shorter)

    self.flag_values.__delattr__('a')
    self.flag_values.__delattr__('abc')
    self.flag_values.__delattr__('common_a_string')
    self.flag_values.__delattr__('common_b_boolean')
    self.flag_values.__delattr__('common_c_boolean')
    self.flag_values.__delattr__('common')
    self.flag_values.__delattr__('commonly')
    self.flag_values.__delattr__('zz')
    self.flag_values.__delattr__('nozz')

  def test_twodasharg_first(self):
    gflags.DEFINE_string("twodash_name", "Bob", "namehelp",
                        flag_values=self.flag_values)
    gflags.DEFINE_string("twodash_blame", "Rob", "blamehelp",
                        flag_values=self.flag_values)
    argv = ('./program',
            '--',
            '--twodash_name=Harry')
    argv = self.flag_values(argv)
    self.assertEqual('Bob', self.flag_values.twodash_name)
    self.assertEqual(argv[1], '--twodash_name=Harry')

  def test_twodasharg_middle(self):
    gflags.DEFINE_string("twodash2_name", "Bob", "namehelp",
                        flag_values=self.flag_values)
    gflags.DEFINE_string("twodash2_blame", "Rob", "blamehelp",
                        flag_values=self.flag_values)
    argv = ('./program',
            '--twodash2_blame=Larry',
            '--',
            '--twodash2_name=Harry')
    argv = self.flag_values(argv)
    self.assertEqual('Bob', self.flag_values.twodash2_name)
    self.assertEqual('Larry', self.flag_values.twodash2_blame)
    self.assertEqual(argv[1], '--twodash2_name=Harry')

  def test_onedasharg_first(self):
    gflags.DEFINE_string("onedash_name", "Bob", "namehelp",
                        flag_values=self.flag_values)
    gflags.DEFINE_string("onedash_blame", "Rob", "blamehelp",
                        flag_values=self.flag_values)
    argv = ('./program',
            '-',
            '--onedash_name=Harry')
    argv = self.flag_values(argv)
    self.assertEqual(argv[1], '-')
    # TODO(csilvers): we should still parse --onedash_name=Harry as a
    # flag, but currently we don't (we stop flag processing as soon as
    # we see the first non-flag).
    # - This requires gnu_getopt from Python 2.3+ see FLAGS.UseGnuGetOpt()

  def test_unrecognized_flags(self):
    gflags.DEFINE_string("name", "Bob", "namehelp", flag_values=self.flag_values)
    # Unknown flag --nosuchflag
    try:
      argv = ('./program', '--nosuchflag', '--name=Bob', 'extra')
      self.flag_values(argv)
      raise AssertionError("Unknown flag exception not raised")
    except gflags.UnrecognizedFlag, e:
      assert e.flagname == 'nosuchflag'
      assert e.flagvalue == '--nosuchflag'

    # Unknown flag -w (short option)
    try:
      argv = ('./program', '-w', '--name=Bob', 'extra')
      self.flag_values(argv)
      raise AssertionError("Unknown flag exception not raised")
    except gflags.UnrecognizedFlag, e:
      assert e.flagname == 'w'
      assert e.flagvalue == '-w'

    # Unknown flag --nosuchflagwithparam=foo
    try:
      argv = ('./program', '--nosuchflagwithparam=foo', '--name=Bob', 'extra')
      self.flag_values(argv)
      raise AssertionError("Unknown flag exception not raised")
    except gflags.UnrecognizedFlag, e:
      assert e.flagname == 'nosuchflagwithparam'
      assert e.flagvalue == '--nosuchflagwithparam=foo'

    # Allow unknown flag --nosuchflag if specified with undefok
    argv = ('./program', '--nosuchflag', '--name=Bob',
            '--undefok=nosuchflag', 'extra')
    argv = self.flag_values(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"

    # Allow unknown flag --noboolflag if undefok=boolflag is specified
    argv = ('./program', '--noboolflag', '--name=Bob',
            '--undefok=boolflag', 'extra')
    argv = self.flag_values(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"

    # But not if the flagname is misspelled:
    try:
      argv = ('./program', '--nosuchflag', '--name=Bob',
              '--undefok=nosuchfla', 'extra')
      self.flag_values(argv)
      raise AssertionError("Unknown flag exception not raised")
    except gflags.UnrecognizedFlag, e:
      assert e.flagname == 'nosuchflag'

    try:
      argv = ('./program', '--nosuchflag', '--name=Bob',
              '--undefok=nosuchflagg', 'extra')
      self.flag_values(argv)
      raise AssertionError("Unknown flag exception not raised")
    except gflags.UnrecognizedFlag, e:
      assert e.flagname == 'nosuchflag'

    # Allow unknown short flag -w if specified with undefok
    argv = ('./program', '-w', '--name=Bob', '--undefok=w', 'extra')
    argv = self.flag_values(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"

    # Allow unknown flag --nosuchflagwithparam=foo if specified
    # with undefok
    argv = ('./program', '--nosuchflagwithparam=foo', '--name=Bob',
            '--undefok=nosuchflagwithparam', 'extra')
    argv = self.flag_values(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"

    # Even if undefok specifies multiple flags
    argv = ('./program', '--nosuchflag', '-w', '--nosuchflagwithparam=foo',
            '--name=Bob',
            '--undefok=nosuchflag,w,nosuchflagwithparam',
            'extra')
    argv = self.flag_values(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"

    # However, not if undefok doesn't specify the flag
    try:
      argv = ('./program', '--nosuchflag', '--name=Bob',
              '--undefok=another_such', 'extra')
      self.flag_values(argv)
      raise AssertionError("Unknown flag exception not raised")
    except gflags.UnrecognizedFlag, e:
      assert e.flagname == 'nosuchflag'

    # Make sure --undefok doesn't mask other option errors.
    try:
      # Provide an option requiring a parameter but not giving it one.
      argv = ('./program', '--undefok=name', '--name')
      self.flag_values(argv)
      raise AssertionError("Missing option parameter exception not raised")
    except gflags.UnrecognizedFlag:
      raise AssertionError("Wrong kind of error exception raised")
    except gflags.FlagsError:
      pass

    # Test --undefok <list>
    argv = ('./program', '--nosuchflag', '-w', '--nosuchflagwithparam=foo',
            '--name=Bob',
            '--undefok',
            'nosuchflag,w,nosuchflagwithparam',
            'extra')
    argv = self.flag_values(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"


class NonGlobalFlagsTest(googletest.TestCase):

  def test_nonglobal_flags(self):
    """Test use of non-global FlagValues"""
    nonglobal_flags = gflags.FlagValues()
    gflags.DEFINE_string("nonglobal_flag", "Bob", "flaghelp", nonglobal_flags)
    argv = ('./program',
            '--nonglobal_flag=Mary',
            'extra')
    argv = nonglobal_flags(argv)
    assert len(argv) == 2, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"
    assert argv[1]=='extra', "extra argument not preserved"
    assert nonglobal_flags['nonglobal_flag'].value == 'Mary'

  def test_unrecognized_nonglobal_flags(self):
    """Test unrecognized non-global flags"""
    nonglobal_flags = gflags.FlagValues()
    argv = ('./program',
            '--nosuchflag')
    try:
      argv = nonglobal_flags(argv)
      raise AssertionError("Unknown flag exception not raised")
    except gflags.UnrecognizedFlag, e:
      assert e.flagname == 'nosuchflag'
      pass

    argv = ('./program',
            '--nosuchflag',
            '--undefok=nosuchflag')

    argv = nonglobal_flags(argv)
    assert len(argv) == 1, "wrong number of arguments pulled"
    assert argv[0]=='./program', "program name not preserved"

  def test_create_flag_errors(self):
    # Since the exception classes are exposed, nothing stops users
    # from creating their own instances. This test makes sure that
    # people modifying the flags module understand that the external
    # mechanisms for creating the exceptions should continue to work.
    e = gflags.FlagsError()
    e = gflags.FlagsError("message")
    e = gflags.DuplicateFlag()
    e = gflags.DuplicateFlag("message")
    e = gflags.IllegalFlagValue()
    e = gflags.IllegalFlagValue("message")
    e = gflags.UnrecognizedFlag()
    e = gflags.UnrecognizedFlag("message")

  def testFlagValuesDelAttr(self):
    """Checks that del self.flag_values.flag_id works."""
    default_value = 'default value for testFlagValuesDelAttr'
    # 1. Declare and delete a flag with no short name.
    flag_values = gflags.FlagValues()
    gflags.DEFINE_string('delattr_foo', default_value, 'A simple flag.',
                        flag_values=flag_values)
    self.assertEquals(flag_values.delattr_foo, default_value)
    flag_obj = flag_values['delattr_foo']
    # We also check that _FlagIsRegistered works as expected :)
    self.assertTrue(flag_values._FlagIsRegistered(flag_obj))
    del flag_values.delattr_foo
    self.assertFalse('delattr_foo' in flag_values.FlagDict())
    self.assertFalse(flag_values._FlagIsRegistered(flag_obj))
    # If the previous del FLAGS.delattr_foo did not work properly, the
    # next definition will trigger a redefinition error.
    gflags.DEFINE_integer('delattr_foo', 3, 'A simple flag.',
                         flag_values=flag_values)
    del flag_values.delattr_foo

    self.assertFalse('delattr_foo' in flag_values.RegisteredFlags())

    # 2. Declare and delete a flag with a short name.
    gflags.DEFINE_string('delattr_bar', default_value, 'flag with short name',
                        short_name='x5', flag_values=flag_values)
    flag_obj = flag_values['delattr_bar']
    self.assertTrue(flag_values._FlagIsRegistered(flag_obj))
    del flag_values.x5
    self.assertTrue(flag_values._FlagIsRegistered(flag_obj))
    del flag_values.delattr_bar
    self.assertFalse(flag_values._FlagIsRegistered(flag_obj))

    # 3. Just like 2, but del flag_values.name last
    gflags.DEFINE_string('delattr_bar', default_value, 'flag with short name',
                        short_name='x5', flag_values=flag_values)
    flag_obj = flag_values['delattr_bar']
    self.assertTrue(flag_values._FlagIsRegistered(flag_obj))
    del flag_values.delattr_bar
    self.assertTrue(flag_values._FlagIsRegistered(flag_obj))
    del flag_values.x5
    self.assertFalse(flag_values._FlagIsRegistered(flag_obj))

    self.assertFalse('delattr_bar' in flag_values.RegisteredFlags())
    self.assertFalse('x5' in flag_values.RegisteredFlags())


class KeyFlagsTest(googletest.TestCase):

  def setUp(self):
    self.flag_values = gflags.FlagValues()

  def _GetNamesOfDefinedFlags(self, module, flag_values):
    """Returns the list of names of flags defined by a module.

    Auxiliary for the testKeyFlags* methods.

    Args:
      module: A module object or a string module name.
      flag_values: A FlagValues object.

    Returns:
      A list of strings.
    """
    return [f.name for f in flag_values._GetFlagsDefinedByModule(module)]

  def _GetNamesOfKeyFlags(self, module, flag_values):
    """Returns the list of names of key flags for a module.

    Auxiliary for the testKeyFlags* methods.

    Args:
      module: A module object or a string module name.
      flag_values: A FlagValues object.

    Returns:
      A list of strings.
    """
    return [f.name for f in flag_values._GetKeyFlagsForModule(module)]

  def _AssertListsHaveSameElements(self, list_1, list_2):
    # Checks that two lists have the same elements with the same
    # multiplicity, in possibly different order.
    list_1 = list(list_1)
    list_1.sort()
    list_2 = list(list_2)
    list_2.sort()
    self.assertListEqual(list_1, list_2)

  def testKeyFlags(self):
    # Before starting any testing, make sure no flags are already
    # defined for module_foo and module_bar.
    self.assertListEqual(self._GetNamesOfKeyFlags(module_foo, self.flag_values),
                         [])
    self.assertListEqual(self._GetNamesOfKeyFlags(module_bar, self.flag_values),
                         [])
    self.assertListEqual(self._GetNamesOfDefinedFlags(module_foo,
                                                      self.flag_values),
                         [])
    self.assertListEqual(self._GetNamesOfDefinedFlags(module_bar,
                                                      self.flag_values),
                         [])

    # Defines a few flags in module_foo and module_bar.
    module_foo.DefineFlags(flag_values=self.flag_values)

    try:
      # Part 1. Check that all flags defined by module_foo are key for
      # that module, and similarly for module_bar.
      for module in [module_foo, module_bar]:
        self._AssertListsHaveSameElements(
            self.flag_values._GetFlagsDefinedByModule(module),
            self.flag_values._GetKeyFlagsForModule(module))
        # Also check that each module defined the expected flags.
        self._AssertListsHaveSameElements(
            self._GetNamesOfDefinedFlags(module, self.flag_values),
            module.NamesOfDefinedFlags())

      # Part 2. Check that gflags.DECLARE_key_flag works fine.
      # Declare that some flags from module_bar are key for
      # module_foo.
      module_foo.DeclareKeyFlags(flag_values=self.flag_values)

      # Check that module_foo has the expected list of defined flags.
      self._AssertListsHaveSameElements(
          self._GetNamesOfDefinedFlags(module_foo, self.flag_values),
          module_foo.NamesOfDefinedFlags())

      # Check that module_foo has the expected list of key flags.
      self._AssertListsHaveSameElements(
          self._GetNamesOfKeyFlags(module_foo, self.flag_values),
          module_foo.NamesOfDeclaredKeyFlags())

      # Part 3. Check that gflags.ADOPT_module_key_flags works fine.
      # Trigger a call to gflags.ADOPT_module_key_flags(module_bar)
      # inside module_foo.  This should declare a few more key
      # flags in module_foo.
      module_foo.DeclareExtraKeyFlags(flag_values=self.flag_values)

      # Check that module_foo has the expected list of key flags.
      self._AssertListsHaveSameElements(
          self._GetNamesOfKeyFlags(module_foo, self.flag_values),
          module_foo.NamesOfDeclaredKeyFlags() +
          module_foo.NamesOfDeclaredExtraKeyFlags())
    finally:
      module_foo.RemoveFlags(flag_values=self.flag_values)

  def testKeyFlagsWithNonDefaultFlagValuesObject(self):
    # Check that key flags work even when we use a FlagValues object
    # that is not the default gflags.self.flag_values object.  Otherwise, this
    # test is similar to testKeyFlags, but it uses only module_bar.
    # The other test module (module_foo) uses only the default values
    # for the flag_values keyword arguments.  This way, testKeyFlags
    # and this method test both the default FlagValues, the explicitly
    # specified one, and a mixed usage of the two.

    # A brand-new FlagValues object, to use instead of gflags.self.flag_values.
    fv = gflags.FlagValues()

    # Before starting any testing, make sure no flags are already
    # defined for module_foo and module_bar.
    self.assertListEqual(
        self._GetNamesOfKeyFlags(module_bar, fv),
        [])
    self.assertListEqual(
        self._GetNamesOfDefinedFlags(module_bar, fv),
        [])

    module_bar.DefineFlags(flag_values=fv)

    # Check that all flags defined by module_bar are key for that
    # module, and that module_bar defined the expected flags.
    self._AssertListsHaveSameElements(
        fv._GetFlagsDefinedByModule(module_bar),
        fv._GetKeyFlagsForModule(module_bar))
    self._AssertListsHaveSameElements(
        self._GetNamesOfDefinedFlags(module_bar, fv),
        module_bar.NamesOfDefinedFlags())

    # Pick two flags from module_bar, declare them as key for the
    # current (i.e., main) module (via gflags.DECLARE_key_flag), and
    # check that we get the expected effect.  The important thing is
    # that we always use flags_values=fv (instead of the default
    # self.flag_values).
    main_module = gflags._GetMainModule()
    names_of_flags_defined_by_bar = module_bar.NamesOfDefinedFlags()
    flag_name_0 = names_of_flags_defined_by_bar[0]
    flag_name_2 = names_of_flags_defined_by_bar[2]

    gflags.DECLARE_key_flag(flag_name_0, flag_values=fv)
    self._AssertListsHaveSameElements(
        self._GetNamesOfKeyFlags(main_module, fv),
        [flag_name_0])

    gflags.DECLARE_key_flag(flag_name_2, flag_values=fv)
    self._AssertListsHaveSameElements(
        self._GetNamesOfKeyFlags(main_module, fv),
        [flag_name_0, flag_name_2])

    # Try with a special (not user-defined) flag too:
    gflags.DECLARE_key_flag('undefok', flag_values=fv)
    self._AssertListsHaveSameElements(
        self._GetNamesOfKeyFlags(main_module, fv),
        [flag_name_0, flag_name_2, 'undefok'])

    gflags.ADOPT_module_key_flags(module_bar, fv)
    self._AssertListsHaveSameElements(
        self._GetNamesOfKeyFlags(main_module, fv),
        names_of_flags_defined_by_bar + ['undefok'])

    # Adopt key flags from the flags module itself.
    gflags.ADOPT_module_key_flags(gflags, flag_values=fv)
    self._AssertListsHaveSameElements(
        self._GetNamesOfKeyFlags(main_module, fv),
        names_of_flags_defined_by_bar + ['flagfile', 'undefok'])

  def testMainModuleHelpWithKeyFlags(self):
    # Similar to test_main_module_help, but this time we make sure to
    # declare some key flags.

    # Safety check that the main module does not declare any flags
    # at the beginning of this test.
    expected_help = ''
    self.assertMultiLineEqual(expected_help, self.flag_values.MainModuleHelp())

    # Define one flag in this main module and some flags in modules
    # a and b.  Also declare one flag from module a and one flag
    # from module b as key flags for the main module.
    gflags.DEFINE_integer('main_module_int_fg', 1,
                         'Integer flag in the main module.',
                         flag_values=self.flag_values)

    try:
      main_module_int_fg_help = (
          "  --main_module_int_fg: Integer flag in the main module.\n"
          "    (default: '1')\n"
          "    (an integer)")

      expected_help += "\n%s:\n%s" % (sys.argv[0], main_module_int_fg_help)
      self.assertMultiLineEqual(expected_help,
                                self.flag_values.MainModuleHelp())

      # The following call should be a no-op: any flag declared by a
      # module is automatically key for that module.
      gflags.DECLARE_key_flag('main_module_int_fg', flag_values=self.flag_values)
      self.assertMultiLineEqual(expected_help,
                                self.flag_values.MainModuleHelp())

      # The definition of a few flags in an imported module should not
      # change the main module help.
      module_foo.DefineFlags(flag_values=self.flag_values)
      self.assertMultiLineEqual(expected_help,
                                self.flag_values.MainModuleHelp())

      gflags.DECLARE_key_flag('tmod_foo_bool', flag_values=self.flag_values)
      tmod_foo_bool_help = (
          "  --[no]tmod_foo_bool: Boolean flag from module foo.\n"
          "    (default: 'true')")
      expected_help += "\n" + tmod_foo_bool_help
      self.assertMultiLineEqual(expected_help,
                                self.flag_values.MainModuleHelp())

      gflags.DECLARE_key_flag('tmod_bar_z', flag_values=self.flag_values)
      tmod_bar_z_help = (
          "  --[no]tmod_bar_z: Another boolean flag from module bar.\n"
          "    (default: 'false')")
      # Unfortunately, there is some flag sorting inside
      # MainModuleHelp, so we can't keep incrementally extending
      # the expected_help string ...
      expected_help = ("\n%s:\n%s\n%s\n%s" %
                       (sys.argv[0],
                        main_module_int_fg_help,
                        tmod_bar_z_help,
                        tmod_foo_bool_help))
      self.assertMultiLineEqual(self.flag_values.MainModuleHelp(),
                                expected_help)

    finally:
      # At the end, delete all the flag information we created.
      self.flag_values.__delattr__('main_module_int_fg')
      module_foo.RemoveFlags(flag_values=self.flag_values)

  def test_ADOPT_module_key_flags(self):
    # Check that ADOPT_module_key_flags raises an exception when
    # called with a module name (as opposed to a module object).
    self.assertRaises(gflags.FlagsError,
                      gflags.ADOPT_module_key_flags,
                      'pyglib.app')


class GetCallingModuleTest(googletest.TestCase):
  """Test whether we correctly determine the module which defines the flag."""

  def test_GetCallingModule(self):
    self.assertEqual(gflags._GetCallingModule(), sys.argv[0])
    self.assertEqual(
        module_foo.GetModuleName(),
        'flags_modules_for_testing.module_foo')
    self.assertEqual(
        module_bar.GetModuleName(),
        'flags_modules_for_testing.module_bar')

    # We execute the following exec statements for their side-effect
    # (i.e., not raising an error).  They emphasize the case that not
    # all code resides in one of the imported modules: Python is a
    # really dynamic language, where we can dynamically construct some
    # code and execute it.
    code = ("import gflags\n"
            "module_name = gflags._GetCallingModule()")
    exec(code)

    # Next two exec statements executes code with a global environment
    # that is different from the global environment of any imported
    # module.
    exec(code, {})
    # vars(self) returns a dictionary corresponding to the symbol
    # table of the self object.  dict(...) makes a distinct copy of
    # this dictionary, such that any new symbol definition by the
    # exec-ed code (e.g., import flags, module_name = ...) does not
    # affect the symbol table of self.
    exec(code, dict(vars(self)))

    # Next test is actually more involved: it checks not only that
    # _GetCallingModule does not crash inside exec code, it also checks
    # that it returns the expected value: the code executed via exec
    # code is treated as being executed by the current module.  We
    # check it twice: first time by executing exec from the main
    # module, second time by executing it from module_bar.
    global_dict = {}
    exec(code, global_dict)
    self.assertEqual(global_dict['module_name'],
                     sys.argv[0])

    global_dict = {}
    module_bar.ExecuteCode(code, global_dict)
    self.assertEqual(
        global_dict['module_name'],
        'flags_modules_for_testing.module_bar')

  def test_GetCallingModuleWithIteritemsError(self):
    # This test checks that _GetCallingModule is using
    # sys.modules.items(), instead of .iteritems().
    orig_sys_modules = sys.modules

    # Mock sys.modules: simulates error produced by importing a module
    # in paralel with our iteration over sys.modules.iteritems().
    class SysModulesMock(dict):
      def __init__(self, original_content):
        dict.__init__(self, original_content)

      def iteritems(self):
        # Any dictionary method is fine, but not .iteritems().
        raise RuntimeError('dictionary changed size during iteration')

    sys.modules = SysModulesMock(orig_sys_modules)
    try:
      # _GetCallingModule should still work as expected:
      self.assertEqual(gflags._GetCallingModule(), sys.argv[0])
      self.assertEqual(
          module_foo.GetModuleName(),
          'flags_modules_for_testing.module_foo')
    finally:
      sys.modules = orig_sys_modules


class FindModuleTest(googletest.TestCase):
  """Testing methods that find a module that defines a given flag."""

  def testFindModuleDefiningFlag(self):
    self.assertEqual('default', FLAGS.FindModuleDefiningFlag(
        '__NON_EXISTENT_FLAG__', 'default'))
    self.assertEqual(
        module_baz.__name__, FLAGS.FindModuleDefiningFlag('tmod_baz_x'))

  def testFindModuleIdDefiningFlag(self):
    self.assertEqual('default', FLAGS.FindModuleIdDefiningFlag(
        '__NON_EXISTENT_FLAG__', 'default'))
    self.assertEqual(
        id(module_baz), FLAGS.FindModuleIdDefiningFlag('tmod_baz_x'))


class FlagsErrorMessagesTest(googletest.TestCase):
  """Testing special cases for integer and float flags error messages."""

  def setUp(self):
    # make sure we are using the old, stupid way of parsing flags.
    self.flag_values = gflags.FlagValues()
    self.flag_values.UseGnuGetOpt(False)

  def testIntegerErrorText(self):
    # Make sure we get proper error text
    gflags.DEFINE_integer('positive', 4, 'non-negative flag', lower_bound=1,
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('non_negative', 4, 'positive flag', lower_bound=0,
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('negative', -4, 'negative flag', upper_bound=-1,
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('non_positive', -4, 'non-positive flag', upper_bound=0,
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('greater', 19, 'greater-than flag', lower_bound=4,
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('smaller', -19, 'smaller-than flag', upper_bound=4,
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('usual', 4, 'usual flag', lower_bound=0,
                         upper_bound=10000, flag_values=self.flag_values)
    gflags.DEFINE_integer('another_usual', 0, 'usual flag', lower_bound=-1,
                         upper_bound=1, flag_values=self.flag_values)

    self._CheckErrorMessage('positive', -4, 'a positive integer')
    self._CheckErrorMessage('non_negative', -4, 'a non-negative integer')
    self._CheckErrorMessage('negative', 0, 'a negative integer')
    self._CheckErrorMessage('non_positive', 4, 'a non-positive integer')
    self._CheckErrorMessage('usual', -4, 'an integer in the range [0, 10000]')
    self._CheckErrorMessage('another_usual', 4,
                            'an integer in the range [-1, 1]')
    self._CheckErrorMessage('greater', -5, 'integer >= 4')
    self._CheckErrorMessage('smaller', 5, 'integer <= 4')

  def testFloatErrorText(self):
    gflags.DEFINE_float('positive', 4, 'non-negative flag', lower_bound=1,
                       flag_values=self.flag_values)
    gflags.DEFINE_float('non_negative', 4, 'positive flag', lower_bound=0,
                       flag_values=self.flag_values)
    gflags.DEFINE_float('negative', -4, 'negative flag', upper_bound=-1,
                       flag_values=self.flag_values)
    gflags.DEFINE_float('non_positive', -4, 'non-positive flag', upper_bound=0,
                       flag_values=self.flag_values)
    gflags.DEFINE_float('greater', 19, 'greater-than flag', lower_bound=4,
                       flag_values=self.flag_values)
    gflags.DEFINE_float('smaller', -19, 'smaller-than flag', upper_bound=4,
                       flag_values=self.flag_values)
    gflags.DEFINE_float('usual', 4, 'usual flag', lower_bound=0,
                       upper_bound=10000, flag_values=self.flag_values)
    gflags.DEFINE_float('another_usual', 0, 'usual flag', lower_bound=-1,
                       upper_bound=1, flag_values=self.flag_values)

    self._CheckErrorMessage('positive', 0.5, 'number >= 1')
    self._CheckErrorMessage('non_negative', -4.0, 'a non-negative number')
    self._CheckErrorMessage('negative', 0.5, 'number <= -1')
    self._CheckErrorMessage('non_positive', 4.0, 'a non-positive number')
    self._CheckErrorMessage('usual', -4.0, 'a number in the range [0, 10000]')
    self._CheckErrorMessage('another_usual', 4.0,
                            'a number in the range [-1, 1]')
    self._CheckErrorMessage('smaller', 5.0, 'number <= 4')

  def _CheckErrorMessage(self, flag_name, flag_value, expected_message_suffix):
    """Set a flag to a given value and make sure we get expected message."""

    try:
      self.flag_values.__setattr__(flag_name, flag_value)
      raise AssertionError('Bounds exception not raised!')
    except gflags.IllegalFlagValue, e:
      expected = ('flag --%(name)s=%(value)s: %(value)s is not %(suffix)s' %
                  {'name': flag_name, 'value': flag_value,
                   'suffix': expected_message_suffix})
      self.assertEquals(str(e), expected)


def main():
  googletest.main()


if __name__ == '__main__':
  main()
