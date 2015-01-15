#!/usr/bin/env python
#*-* coding: utf-8
"""Closure typeannotation parsing and utilities."""



from closure_linter import errors
from closure_linter import javascripttokens
from closure_linter.common import error

# Shorthand
TYPE = javascripttokens.JavaScriptTokenType


class TypeAnnotation(object):
  """Represents a structured view of a closure type annotation.

  Attribute:
    identifier: The name of the type.
    key_type: The name part before a colon.
    sub_types: The list of sub_types used e.g. for Array.<…>
    or_null: The '?' annotation
    not_null: The '!' annotation
    type_group: If this a a grouping (a|b), but does not include function(a).
    return_type: The return type of a function definition.
    alias: The actual type set by closurizednamespaceinfo if the identifier uses
        an alias to shorten the name.
    tokens: An ordered list of tokens used for this type. May contain
        TypeAnnotation instances for sub_types, key_type or return_type.
  """

  IMPLICIT_TYPE_GROUP = 2

  NULLABILITY_UNKNOWN = 2

  # Frequently used known non-nullable types.
  NON_NULLABLE = frozenset([
      'boolean', 'function', 'number', 'string', 'undefined'])
  # Frequently used known nullable types.
  NULLABLE_TYPE_WHITELIST = frozenset([
      'Array', 'Document', 'Element', 'Function', 'Node', 'NodeList',
      'Object'])

  def __init__(self):
    self.identifier = ''
    self.sub_types = []
    self.or_null = False
    self.not_null = False
    self.type_group = False
    self.alias = None
    self.key_type = None
    self.record_type = False
    self.opt_arg = False
    self.return_type = None
    self.tokens = []

  def IsFunction(self):
    """Determines whether this is a function definition."""
    return self.identifier == 'function'

  def IsConstructor(self):
    """Determines whether this is a function definition for a constructor."""
    key_type = self.sub_types and self.sub_types[0].key_type
    return self.IsFunction() and key_type.identifier == 'new'

  def IsRecordType(self):
    """Returns True if this type is a record type."""
    return (self.record_type or
            bool([t for t in self.sub_types if t.IsRecordType()]))

  def IsVarArgsType(self):
    """Determines if the type is a var_args type, i.e. starts with '...'."""
    return self.identifier.startswith('...') or (
        self.type_group == TypeAnnotation.IMPLICIT_TYPE_GROUP and
        self.sub_types[0].identifier.startswith('...'))

  def IsEmpty(self):
    """Returns True if the type is empty."""
    return not self.tokens

  def IsUnknownType(self):
    """Returns True if this is the unknown type {?}."""
    return (self.or_null
            and not self.identifier
            and not self.sub_types
            and not self.return_type)

  def Append(self, item):
    """Adds a sub_type to this type and finalizes it.

    Args:
      item: The TypeAnnotation item to append.
    """
    # item is a TypeAnnotation instance, so pylint: disable=protected-access
    self.sub_types.append(item._Finalize(self))

  def __repr__(self):
    """Reconstructs the type definition."""
    append = ''
    if self.sub_types:
      separator = (',' if not self.type_group else '|')
      if self.identifier == 'function':
        surround = '(%s)'
      else:
        surround = {False: '{%s}' if self.record_type else '<%s>',
                    True: '(%s)',
                    self.IMPLICIT_TYPE_GROUP: '%s'}[self.type_group]
      append = surround % separator.join([repr(t) for t in self.sub_types])
    if self.return_type:
      append += ':%s' % repr(self.return_type)
    append += '=' if self.opt_arg else ''
    prefix = '' + ('?' if self.or_null else '') + ('!' if self.not_null else '')
    keyword = '%s:' % repr(self.key_type) if self.key_type else ''
    return keyword + prefix + '%s' % (self.alias or self.identifier) + append

  def ToString(self):
    """Concats the type's tokens to form a string again."""
    ret = []
    for token in self.tokens:
      if not isinstance(token, TypeAnnotation):
        ret.append(token.string)
      else:
        ret.append(token.ToString())
    return ''.join(ret)

  def Dump(self, indent=''):
    """Dumps this type's structure for debugging purposes."""
    result = []
    for t in self.tokens:
      if isinstance(t, TypeAnnotation):
        result.append(indent + str(t) + ' =>\n' + t.Dump(indent + '  '))
      else:
        result.append(indent + str(t))
    return '\n'.join(result)

  def IterIdentifiers(self):
    """Iterates over all identifiers in this type and its subtypes."""
    if self.identifier:
      yield self.identifier
    for subtype in self.IterTypes():
      for identifier in subtype.IterIdentifiers():
        yield identifier

  def IterTypeGroup(self):
    """Iterates over all types in the type group including self.

    Yields:
      If this is a implicit or manual type-group: all sub_types.
      Otherwise: self
      E.g. for @type {Foo.<Bar>} this will yield only Foo.<Bar>,
      for @type {Foo|(Bar|Sample)} this will yield Foo, Bar and Sample.

    """
    if self.type_group:
      for sub_type in self.sub_types:
        for sub_type in sub_type.IterTypeGroup():
          yield sub_type
    else:
      yield self

  def IterTypes(self):
    """Iterates over each subtype as well as return and key types."""
    if self.return_type:
      yield self.return_type

    if self.key_type:
      yield self.key_type

    for sub_type in self.sub_types:
      yield sub_type

  def GetNullability(self, modifiers=True):
    """Computes whether the type may be null.

    Args:
      modifiers: Whether the modifiers ? and ! should be considered in the
                 evaluation.
    Returns:
      True if the type allows null, False if the type is strictly non nullable
      and NULLABILITY_UNKNOWN if the nullability cannot be determined.
    """

    # Explicitly marked nullable types or 'null' are nullable.
    if (modifiers and self.or_null) or self.identifier == 'null':
      return True

    # Explicitly marked non-nullable types or non-nullable base types:
    if ((modifiers and self.not_null) or self.record_type
        or self.identifier in self.NON_NULLABLE):
      return False

    # A type group is nullable if any of its elements are nullable.
    if self.type_group:
      maybe_nullable = False
      for sub_type in self.sub_types:
        nullability = sub_type.GetNullability()
        if nullability == self.NULLABILITY_UNKNOWN:
          maybe_nullable = nullability
        elif nullability:
          return True
      return maybe_nullable

    # Whitelisted types are nullable.
    if self.identifier.rstrip('.') in self.NULLABLE_TYPE_WHITELIST:
      return True

    # All other types are unknown (most should be nullable, but
    # enums are not and typedefs might not be).
    return self.NULLABILITY_UNKNOWN

  def WillAlwaysBeNullable(self):
    """Computes whether the ! flag is illegal for this type.

    This is the case if this type or any of the subtypes is marked as
    explicitly nullable.

    Returns:
      True if the ! flag would be illegal.
    """
    if self.or_null or self.identifier == 'null':
      return True

    if self.type_group:
      return bool([t for t in self.sub_types if t.WillAlwaysBeNullable()])

    return False

  def _Finalize(self, parent):
    """Fixes some parsing issues once the TypeAnnotation is complete."""

    # Normalize functions whose definition ended up in the key type because
    # they defined a return type after a colon.
    if self.key_type and self.key_type.identifier == 'function':
      current = self.key_type
      current.return_type = self
      self.key_type = None
      # opt_arg never refers to the return type but to the function itself.
      current.opt_arg = self.opt_arg
      self.opt_arg = False
      return current

    # If a typedef just specified the key, it will not end up in the key type.
    if parent.record_type and not self.key_type:
      current = TypeAnnotation()
      current.key_type = self
      current.tokens.append(self)
      return current
    return self

  def FirstToken(self):
    """Returns the first token used in this type or any of its subtypes."""
    first = self.tokens[0]
    return first.FirstToken() if isinstance(first, TypeAnnotation) else first


def Parse(token, token_end, error_handler):
  """Parses a type annotation and returns a TypeAnnotation object."""
  return TypeAnnotationParser(error_handler).Parse(token.next, token_end)


class TypeAnnotationParser(object):
  """A parser for type annotations constructing the TypeAnnotation object."""

  def __init__(self, error_handler):
    self._stack = []
    self._error_handler = error_handler
    self._closing_error = False

  def Parse(self, token, token_end):
    """Parses a type annotation and returns a TypeAnnotation object."""
    root = TypeAnnotation()
    self._stack.append(root)
    current = TypeAnnotation()
    root.tokens.append(current)

    while token and token != token_end:
      if token.type in (TYPE.DOC_TYPE_START_BLOCK, TYPE.DOC_START_BRACE):
        if token.string == '(':
          if (current.identifier and
              current.identifier not in ['function', '...']):
            self.Error(token,
                       'Invalid identifier for (): "%s"' % current.identifier)
          current.type_group = current.identifier != 'function'
        elif token.string == '{':
          current.record_type = True
        current.tokens.append(token)
        self._stack.append(current)
        current = TypeAnnotation()
        self._stack[-1].tokens.append(current)

      elif token.type in (TYPE.DOC_TYPE_END_BLOCK, TYPE.DOC_END_BRACE):
        prev = self._stack.pop()
        prev.Append(current)
        current = prev

        # If an implicit type group was created, close it as well.
        if prev.type_group == TypeAnnotation.IMPLICIT_TYPE_GROUP:
          prev = self._stack.pop()
          prev.Append(current)
          current = prev
        current.tokens.append(token)

      elif token.type == TYPE.DOC_TYPE_MODIFIER:
        if token.string == '!':
          current.tokens.append(token)
          current.not_null = True
        elif token.string == '?':
          current.tokens.append(token)
          current.or_null = True
        elif token.string == ':':
          current.tokens.append(token)
          prev = current
          current = TypeAnnotation()
          prev.tokens.append(current)
          current.key_type = prev
        elif token.string == '=':
          # For implicit type groups the '=' refers to the parent.
          try:
            if self._stack[-1].type_group == TypeAnnotation.IMPLICIT_TYPE_GROUP:
              self._stack[-1].tokens.append(token)
              self._stack[-1].opt_arg = True
            else:
              current.tokens.append(token)
              current.opt_arg = True
          except IndexError:
            self.ClosingError(token)
        elif token.string == '|':
          # If a type group has explicitly been opened do a normal append.
          # Otherwise we have to open the type group and move the current
          # type into it, before appending
          if not self._stack[-1].type_group:
            type_group = TypeAnnotation()
            if current.key_type and current.key_type.identifier != 'function':
              type_group.key_type = current.key_type
              current.key_type = None
            type_group.type_group = TypeAnnotation.IMPLICIT_TYPE_GROUP
            # Fix the token order
            prev = self._stack[-1].tokens.pop()
            self._stack[-1].tokens.append(type_group)
            type_group.tokens.append(prev)
            self._stack.append(type_group)
          self._stack[-1].tokens.append(token)
          self.Append(current, error_token=token)
          current = TypeAnnotation()
          self._stack[-1].tokens.append(current)
        elif token.string == ',':
          self.Append(current, error_token=token)
          current = TypeAnnotation()
          self._stack[-1].tokens.append(token)
          self._stack[-1].tokens.append(current)
        else:
          current.tokens.append(token)
          self.Error(token, 'Invalid token')

      elif token.type == TYPE.COMMENT:
        current.tokens.append(token)
        current.identifier += token.string.strip()

      elif token.type in [TYPE.DOC_PREFIX, TYPE.WHITESPACE]:
        current.tokens.append(token)

      else:
        current.tokens.append(token)
        self.Error(token, 'Unexpected token')

      token = token.next

    self.Append(current, error_token=token)
    try:
      ret = self._stack.pop()
    except IndexError:
      self.ClosingError(token)
      # The type is screwed up, but let's return something.
      return current

    if self._stack and (len(self._stack) != 1 or
                        ret.type_group != TypeAnnotation.IMPLICIT_TYPE_GROUP):
      self.Error(token, 'Too many opening items.')

    return ret if len(ret.sub_types) > 1 else ret.sub_types[0]

  def Append(self, type_obj, error_token):
    """Appends a new TypeAnnotation object to the current parent."""
    if self._stack:
      self._stack[-1].Append(type_obj)
    else:
      self.ClosingError(error_token)

  def ClosingError(self, token):
    """Reports an error about too many closing items, but only once."""
    if not self._closing_error:
      self._closing_error = True
      self.Error(token, 'Too many closing items.')

  def Error(self, token, message):
    """Calls the error_handler to post an error message."""
    if self._error_handler:
      self._error_handler.HandleError(error.Error(
          errors.JSDOC_DOES_NOT_PARSE,
          'Error parsing jsdoc type at token "%s" (column: %d): %s' %
          (token.string, token.start_index, message), token))
