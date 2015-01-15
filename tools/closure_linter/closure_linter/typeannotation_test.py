#!/usr/bin/env python
"""Unit tests for the typeannotation module."""




import unittest as googletest

from closure_linter import testutil
from closure_linter.common import erroraccumulator

CRAZY_TYPE = ('Array.<!function(new:X,{a:null},...(c|d)):'
              'function(...(Object.<string>))>')


class TypeErrorException(Exception):
  """Exception for TypeErrors."""

  def __init__(self, errors):
    super(TypeErrorException, self).__init__()
    self.errors = errors


class TypeParserTest(googletest.TestCase):
  """Tests for typeannotation parsing."""

  def _ParseComment(self, script):
    """Parse a script that contains one comment and return it."""
    accumulator = erroraccumulator.ErrorAccumulator()
    _, comments = testutil.ParseFunctionsAndComments(script, accumulator)
    if accumulator.GetErrors():
      raise TypeErrorException(accumulator.GetErrors())
    self.assertEquals(1, len(comments))
    return comments[0]

  def _ParseType(self, type_str):
    """Creates a comment to parse and returns the parsed type."""
    comment = self._ParseComment('/** @type {%s} **/' % type_str)
    return comment.GetDocFlags()[0].jstype

  def assertProperReconstruction(self, type_str, matching_str=None):
    """Parses the type and asserts the its repr matches the type.

    If matching_str is specified, it will assert that the repr matches this
    string instead.

    Args:
      type_str: The type string to parse.
      matching_str: A string the __repr__ of the parsed type should match.
    Returns:
      The parsed js_type.
    """
    parsed_type = self._ParseType(type_str)
    # Use listEqual assertion to more easily identify the difference
    self.assertListEqual(list(matching_str or type_str),
                         list(repr(parsed_type)))
    self.assertEquals(matching_str or type_str, repr(parsed_type))

    # Newlines will be inserted by the file writer.
    self.assertEquals(type_str.replace('\n', ''), parsed_type.ToString())
    return parsed_type

  def assertNullable(self, type_str, nullable=True):
    parsed_type = self.assertProperReconstruction(type_str)
    self.assertEquals(nullable, parsed_type.GetNullability(),
                      '"%s" should %sbe nullable' %
                      (type_str, 'not ' if nullable else ''))

  def assertNotNullable(self, type_str):
    return self.assertNullable(type_str, nullable=False)

  def testReconstruction(self):
    self.assertProperReconstruction('*')
    self.assertProperReconstruction('number')
    self.assertProperReconstruction('(((number)))')
    self.assertProperReconstruction('!number')
    self.assertProperReconstruction('?!number')
    self.assertProperReconstruction('number=')
    self.assertProperReconstruction('number=!?', '?!number=')
    self.assertProperReconstruction('number|?string')
    self.assertProperReconstruction('(number|string)')
    self.assertProperReconstruction('?(number|string)')
    self.assertProperReconstruction('Object.<number,string>')
    self.assertProperReconstruction('function(new:Object)')
    self.assertProperReconstruction('function(new:Object):number')
    self.assertProperReconstruction('function(new:Object,Element):number')
    self.assertProperReconstruction('function(this:T,...)')
    self.assertProperReconstruction('{a:?number}')
    self.assertProperReconstruction('{a:?number,b:(number|string)}')
    self.assertProperReconstruction('{c:{nested_element:*}|undefined}')
    self.assertProperReconstruction('{handleEvent:function(?):?}')
    self.assertProperReconstruction('function():?|null')
    self.assertProperReconstruction('null|function():?|bar')

  def testOptargs(self):
    self.assertProperReconstruction('number=')
    self.assertProperReconstruction('number|string=')
    self.assertProperReconstruction('(number|string)=')
    self.assertProperReconstruction('(number|string=)')
    self.assertProperReconstruction('(number=|string)')
    self.assertProperReconstruction('function(...):number=')

  def testIndepth(self):
    # Do an deeper check of the crazy identifier
    crazy = self.assertProperReconstruction(CRAZY_TYPE)
    self.assertEquals('Array.', crazy.identifier)
    self.assertEquals(1, len(crazy.sub_types))
    func1 = crazy.sub_types[0]
    func2 = func1.return_type
    self.assertEquals('function', func1.identifier)
    self.assertEquals('function', func2.identifier)
    self.assertEquals(3, len(func1.sub_types))
    self.assertEquals(1, len(func2.sub_types))
    self.assertEquals('Object.', func2.sub_types[0].sub_types[0].identifier)

  def testIterIdentifiers(self):
    nested_identifiers = self._ParseType('(a|{b:(c|function(new:d):e)})')
    for identifier in ('a', 'b', 'c', 'd', 'e'):
      self.assertIn(identifier, nested_identifiers.IterIdentifiers())

  def testIsEmpty(self):
    self.assertTrue(self._ParseType('').IsEmpty())
    self.assertFalse(self._ParseType('?').IsEmpty())
    self.assertFalse(self._ParseType('!').IsEmpty())
    self.assertFalse(self._ParseType('<?>').IsEmpty())

  def testIsConstructor(self):
    self.assertFalse(self._ParseType('').IsConstructor())
    self.assertFalse(self._ParseType('Array.<number>').IsConstructor())
    self.assertTrue(self._ParseType('function(new:T)').IsConstructor())

  def testIsVarArgsType(self):
    self.assertTrue(self._ParseType('...number').IsVarArgsType())
    self.assertTrue(self._ParseType('...Object|Array').IsVarArgsType())
    self.assertTrue(self._ParseType('...(Object|Array)').IsVarArgsType())
    self.assertFalse(self._ParseType('Object|...Array').IsVarArgsType())
    self.assertFalse(self._ParseType('(...Object|Array)').IsVarArgsType())

  def testIsUnknownType(self):
    self.assertTrue(self._ParseType('?').IsUnknownType())
    self.assertTrue(self._ParseType('Foo.<?>').sub_types[0].IsUnknownType())
    self.assertFalse(self._ParseType('?|!').IsUnknownType())
    self.assertTrue(self._ParseType('?|!').sub_types[0].IsUnknownType())
    self.assertFalse(self._ParseType('!').IsUnknownType())

    long_type = 'function():?|{handleEvent:function(?=):?,sample:?}|?='
    record = self._ParseType(long_type)
    # First check that there's not just one type with 3 return types, but three
    # top-level types.
    self.assertEquals(3, len(record.sub_types))

    # Now extract all unknown type instances and verify that they really are.
    handle_event, sample = record.sub_types[1].sub_types
    for i, sub_type in enumerate([
        record.sub_types[0].return_type,
        handle_event.return_type,
        handle_event.sub_types[0],
        sample,
        record.sub_types[2]]):
      self.assertTrue(sub_type.IsUnknownType(),
                      'Type %d should be the unknown type: %s\n%s' % (
                          i, sub_type.tokens, record.Dump()))

  def testTypedefNames(self):
    easy = self._ParseType('{a}')
    self.assertTrue(easy.record_type)

    easy = self.assertProperReconstruction('{a}', '{a:}').sub_types[0]
    self.assertEquals('a', easy.key_type.identifier)
    self.assertEquals('', easy.identifier)

    easy = self.assertProperReconstruction('{a:b}').sub_types[0]
    self.assertEquals('a', easy.key_type.identifier)
    self.assertEquals('b', easy.identifier)

  def assertTypeError(self, type_str):
    """Asserts that parsing the given type raises a linter error."""
    self.assertRaises(TypeErrorException, self._ParseType, type_str)

  def testParseBadTypes(self):
    """Tests that several errors in types don't break the parser."""
    self.assertTypeError('<')
    self.assertTypeError('>')
    self.assertTypeError('Foo.<Bar')
    self.assertTypeError('Foo.Bar>=')
    self.assertTypeError('Foo.<Bar>>=')
    self.assertTypeError('(')
    self.assertTypeError(')')
    self.assertTypeError('Foo.<Bar)>')
    self._ParseType(':')
    self._ParseType(':foo')
    self.assertTypeError(':)foo')
    self.assertTypeError('(a|{b:(c|function(new:d):e')

  def testNullable(self):
    self.assertNullable('null')
    self.assertNullable('Object')
    self.assertNullable('?string')
    self.assertNullable('?number')

    self.assertNotNullable('string')
    self.assertNotNullable('number')
    self.assertNotNullable('boolean')
    self.assertNotNullable('function(Object)')
    self.assertNotNullable('function(Object):Object')
    self.assertNotNullable('function(?Object):?Object')
    self.assertNotNullable('!Object')

    self.assertNotNullable('boolean|string')
    self.assertNotNullable('(boolean|string)')

    self.assertNullable('(boolean|string|null)')
    self.assertNullable('(?boolean)')
    self.assertNullable('?(boolean)')

    self.assertNullable('(boolean|Object)')
    self.assertNotNullable('(boolean|(string|{a:}))')

  def testSpaces(self):
    """Tests that spaces don't change the outcome."""
    type_str = (' A < b | ( c | ? ! d e f ) > | '
                'function ( x : . . . ) : { y : z = } ')
    two_spaces = type_str.replace(' ', '  ')
    no_spaces = type_str.replace(' ', '')
    newlines = type_str.replace(' ', '\n * ')
    self.assertProperReconstruction(no_spaces)
    self.assertProperReconstruction(type_str, no_spaces)
    self.assertProperReconstruction(two_spaces, no_spaces)
    self.assertProperReconstruction(newlines, no_spaces)

if __name__ == '__main__':
  googletest.main()
