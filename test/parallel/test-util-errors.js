'use strict';

require('../common');
const assert = require('assert');

const errors = require('util/errors');
const {
  makeErrorWithCode,
  AbortError,
  DOMException,
} = errors;

{
  const messages = new Map([
    ['FOO', 'bar %s'],
    ['BAZ', (name) => `${name}`],
    ['ABC', 1],
  ]);

  const E = makeErrorWithCode('FOO', { messages });
  const e = new E('hello');
  assert.strictEqual(e.message, 'bar hello');
  assert.strictEqual(e.code, 'FOO');
  assert(e instanceof Error);

  // Asserts because the message requires arguments that are not given
  assert.throws(() => new E(), {
    code: 'ERR_INTERNAL_ASSERTION',
  });

  const E2 = makeErrorWithCode('BAZ', { Base: TypeError, messages });
  const e2 = new E2('hello');
  assert.strictEqual(e2.message, 'hello');
  assert.strictEqual(e2.code, 'BAZ');
  assert(e2 instanceof TypeError);

  assert.throws(() => makeErrorWithCode('ABC', { messages }), {
    code: 'ERR_INVALID_ERROR_MESSAGE',
  });

  assert.throws(() => makeErrorWithCode('ABC'), {
    code: 'ERR_MISSING_ERROR_MESSAGE',
  });

  assert.throws(() => makeErrorWithCode('XYZ', { messages }), {
    code: 'ERR_MISSING_ERROR_MESSAGE',
  });

  assert.throws(() => makeErrorWithCode(1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => makeErrorWithCode('Test', 1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => makeErrorWithCode('Test', { Base: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => makeErrorWithCode('Test', { messages: 'hello' }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

{
  const err = new AbortError();
  assert.strictEqual(err.name, 'AbortError');
  assert.strictEqual(err.code, 'ABORT_ERR');
}

{
  const kDomExceptions = {
    'IndexSizeError': 1,
    'HierarchyRequestError': 3,
    'WrongDocumentError': 4,
    'InvalidCharacterError': 5,
    'NoModificationAllowedError': 7,
    'NotFoundError': 8,
    'NotSupportedError': 9,
    'InUseAttributeError': 10,
    'InvalidStateError': 11,
    'SyntaxError': 12,
    'InvalidModificationError': 13,
    'NamespaceError': 14,
    'InvalidAccessError': 15,
    'TypeMismatchError': 17,
    'SecurityError': 18,
    'NetworkError': 19,
    'AbortError': 20,
    'URLMismatchError': 21,
    'QuotaExceededError': 22,
    'TimeoutError': 23,
    'InvalidNodeTypeError': 24,
    'DataCloneError': 25,
    'EncodingError': 0,
    'NotReadableError': 0,
    'UnknownError': 0,
    'ConstraintError': 0,
    'DataError': 0,
    'TransactionInactiveError': 0,
    'ReadOnlyError': 0,
    'VersionError': 0,
    'OperationError': 0,
    'NotAllowedError': 0,
  };

  Object.keys(kDomExceptions).forEach((i) => {
    const e = new DOMException('message', i);
    assert.strictEqual(e.name, i);
    assert.strictEqual(e.code, kDomExceptions[i]);
  });
}
