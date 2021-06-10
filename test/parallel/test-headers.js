'use strict';

const common = require('../common');
const assert = require('assert');

// Flags: --expose-internals

const { Headers, kHeadersList } = require('internal/fetch/headers');

{
  // init is undefined
  assert.deepStrictEqual(new Headers()[kHeadersList], []);
  // Init is flat array with one entry
  assert.deepStrictEqual(
    new Headers(['test-name', 'test-value'])[kHeadersList],
    ['test-name', 'test-value']
  );
  // Init is flat array with multiple entries
  assert.deepStrictEqual(
    new Headers(['test-name-1', 'test-value-1', 'test-name-2', 'test-value-2'])[
      kHeadersList
    ],
    ['test-name-1', 'test-value-1', 'test-name-2', 'test-value-2']
  );
  // Init is multidimensional array with one entry
  assert.deepStrictEqual(
    new Headers([['test-name-1', 'test-value-1']])[kHeadersList],
    ['test-name-1', 'test-value-1']
  );
  // Init is multidimensional array with multiple entries
  assert.deepStrictEqual(
    new Headers([
      ['test-name-1', 'test-value-1'],
      ['test-name-2', 'test-value-2'],
    ])[kHeadersList],
    ['test-name-1', 'test-value-1', 'test-name-2', 'test-value-2']
  );
  // Throws when init length is odd
  assert.throws(() => {
    new Headers(['test-name-1', 'test-value', 'test-name-2']);
  }, "Error: The argument 'init' is not even in length. Received [ 'test-name-1', 'test-value', 'test-name-2' ]");
  // Throws when multidimensional init entry length is not 2
  assert.throws(() => {
    new Headers([['test-name-1', 'test-value-1'], ['test-name-2']]);
  }, "Error: The argument 'init' is not of length 2. Received [ 'test-name-2' ]");
  // Throws when init is not valid array input
  assert.throws(() => {
    new Headers([0, 1]);
  }, "Error: The argument 'init' is not a valid array entry. Received [ 0, 1 ]");
}

{
  // Init is object with single entry
  const headers = new Headers({
    'test-name-1': 'test-value-1',
  });
  assert.strictEqual(headers[kHeadersList].length, 2);
}

{
  // Init is object with multiple entries
  const headers = new Headers({
    'test-name-1': 'test-value-1',
    'test-name-2': 'test-value-2',
  });
  assert.strictEqual(headers[kHeadersList].length, 4);
}

{
  // Init fails silently when initialized with BoxedPrimitives
  try {
    new Headers(new Number());
    new Headers(new Boolean());
    new Headers(new String());
  } catch (error) {
    common.mustNotCall(error);
  }
}

{
  // Init fails silently if function or primitive is passed
  try {
    new Headers(Function);
    new Headers(function() {});
    new Headers(1);
    new Headers('test');
  } catch (error) {
    common.mustNotCall(error);
  }
}

{
  // headers append
  const headers = new Headers();
  headers.append('test-name-1', 'test-value-1');
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1',
  ]);
  headers.append('test-name-2', 'test-value-2');
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1',
    'test-name-2',
    'test-value-2',
  ]);
  headers.append('test-name-1', 'test-value-3');
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1, test-value-3',
    'test-name-2',
    'test-value-2',
  ]);

  assert.throws(() => {
    headers.append();
  });

  assert.throws(() => {
    headers.append('test-name');
  });

  assert.throws(() => {
    headers.append('invalid @ header ? name', 'test-value');
  });
}
