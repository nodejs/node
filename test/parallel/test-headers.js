'use strict';

const common = require('../common');
const assert = require('assert');

// Flags: --expose-internals

const { Headers, kHeadersList, binarySearch } = require('internal/fetch/headers');

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
  // append
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

{
  // delete
  const headers = new Headers();
  headers.append('test-name-1', 'test-value-1');
  headers.append('test-name-2', 'test-value-2');
  headers.append('test-name-3', 'test-value-3');
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1',
    'test-name-2',
    'test-value-2',
    'test-name-3',
    'test-value-3'
  ]);
  assert.doesNotThrow(() => headers.delete('test-name-2'))
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1',
    'test-name-3',
    'test-value-3'
  ]);
  assert.doesNotThrow(() => headers.delete('does-not-exist'))
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1',
    'test-name-3',
    'test-value-3'
  ]);

  assert.throws(() => {
    headers.delete()
  })
  
  assert.throws(() => {
    headers.delete('invalid @ header ?')
  })
}

{
  // get
  const headers = new Headers();
  headers.append('test-name-1', 'test-value-1');
  headers.append('test-name-2', 'test-value-2');
  headers.append('test-name-3', 'test-value-3');
  assert.deepStrictEqual(headers.get('test-name-1'), 'test-value-1');
  assert.deepStrictEqual(headers.get('does-not-exist'), null);
  headers.append('test-name-2', 'test-value-4')
  assert.deepStrictEqual(headers.get('test-name-2'), 'test-value-2, test-value-4')

  assert.throws(() => {
    headers.get()
  })
  
  assert.throws(() => {
    headers.get('invalid @ header ?')
  })
}

{
  // has
  const headers = new Headers();
  headers.append('test-name-1', 'test-value-1');
  headers.append('test-name-2', 'test-value-2');
  headers.append('test-name-3', 'test-value-3');
  assert.strictEqual(headers.has('test-name-1'), true);
  assert.strictEqual(headers.has('does-not-exist'), false);

  assert.throws(() => {
    headers.has()
  })
  
  assert.throws(() => {
    headers.has('invalid @ header ?')
  })
}

{
  // set
  const headers = new Headers();
  headers.set('test-name-1', 'test-value-1');
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1',
  ]);
  headers.set('test-name-2', 'test-value-2');
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-1',
    'test-name-2',
    'test-value-2',
  ]);
  headers.set('test-name-1', 'test-value-3');
  assert.deepStrictEqual(headers[kHeadersList], [
    'test-name-1',
    'test-value-3',
    'test-name-2',
    'test-value-2',
  ]);

  assert.throws(() => {
    headers.set();
  });

  assert.throws(() => {
    headers.set('test-name');
  });

  assert.throws(() => {
    headers.set('invalid @ header ? name', 'test-value');
  });
}

{
  // for each
  const init = [
    ['a', '1'],
    ['b', '2'],
    ['c', '3'],
    ['abc', '4'],
    ['b', '5']
  ]
  const expected = [
    ['a', '1'],
    ['abc', '4'],
    ['b', '2, 5'],
    ['c', '3']
  ]

  const headers = new Headers(init)
  const that = {}
  let i = 0
  headers.forEach(function (value, key, _headers) {
    assert.deepStrictEqual(expected[i++], [key, value])
    assert.strictEqual(headers, _headers)
    assert.strictEqual(this, that)
  }, that)
}

{
  // entries
  const init = [
    ['a', '1'],
    ['b', '2'],
    ['c', '3'],
    ['abc', '4'],
    ['b', '5']
  ]
  const expected = [
    ['a', '1'],
    ['abc', '4'],
    ['b', '2, 5'],
    ['c', '3']
  ]
  const headers = new Headers(init)
  let i = 0
  for (const header of headers.entries()) {
    assert.deepStrictEqual(header, expected[i++])
  }
}

{
  // keys
  const init = [
    ['a', '1'],
    ['b', '2'],
    ['c', '3'],
    ['abc', '4'],
    ['b', '5']
  ]
  const expected = ['a', 'abc', 'b', 'c']
  const headers = new Headers(init)
  let i = 0
  for (const key of headers.keys()) {
    assert.deepStrictEqual(key, expected[i++])
  }
}

{
  // values
  const init = [
    ['a', '1'],
    ['b', '2'],
    ['c', '3'],
    ['abc', '4'],
    ['b', '5']
  ]
  const expected = ['1', '4', '2, 5', '3']
  const headers = new Headers(init)
  let i = 0
  for (const value of headers.values()) {
    assert.deepStrictEqual(value, expected[i++])
  }
}

{
  // for of
  const init = [
    ['a', '1'],
    ['b', '2'],
    ['c', '3'],
    ['abc', '4'],
    ['b', '5']
  ]
  const expected = [
    ['a', '1'],
    ['abc', '4'],
    ['b', '2, 5'],
    ['c', '3']
  ]
  let i = 0
  const headers = new Headers(init)
  for (const header of headers) {
    assert.deepStrictEqual(header, expected[i++])
  }
}

{
  //           0   1   2   3   4   5   6   7
  const l1 = ['b', 1, 'c', 2, 'd', 3, 'f', 4]
  //           0   1   2   3   4   5   6   7   8   9
  const l2 = ['b', 1, 'c', 2, 'd', 3, 'e', 4, 'g', 5]
  //           0   1   2   3    4    5   6   7
  const l3 = ['a', 1, 'b', 2, 'bcd', 3, 'c', 4]
  //           0   1   2   3   4   5    6    7   8   9
  const l4 = ['a', 1, 'b', 2, 'c', 3, 'cde', 4, 'f', 5]

  const tests = [
    { input: [l1, 'c'], expected: 2, message: 'find item in n=even array' },
    { input: [l1, 'f'], expected: 6, message: 'find item at end of n=even array' },
    { input: [l1, 'b'], expected: 0, message: 'find item at beg of n=even array' },
    { input: [l1, 'e'], expected: 6, message: 'find new item position in n=even array' },
    { input: [l1, 'g'], expected: 8, message: 'find new item position at end of n=even array' },
    { input: [l1, 'a'], expected: 0, message: 'find new item position at beg of n=even array' },
    { input: [l2, 'c'], expected: 2, message: 'find item in n=odd array' },
    { input: [l2, 'g'], expected: 8, message: 'find item at end of n=odd array' },
    { input: [l2, 'b'], expected: 0, message: 'find item at beg of n=odd array' },
    { input: [l2, 'f'], expected: 8, message: 'find new item position in n=odd array' },
    { input: [l2, 'h'], expected: 10, message: 'find new item position at end of n=odd array' },
    { input: [l2, 'a'], expected: 0, message: 'find new item position at beg of n=odd array' },
    { input: [l3, 'b'], expected: 2, message: 'find item with similarity in n=odd array' },
    { input: [l3, 'bcd'], expected: 4, message: 'find item with similarity in n=odd array' },
    { input: [l4, 'c'], expected: 4, message: 'find item with similarity in n=odd array' },
    { input: [l4, 'cde'], expected: 6, message: 'find item with similarity in n=odd array' }
  ]

  tests.forEach(({ input: [list, target], expected, message }) => {
    assert.deepStrictEqual(expected, binarySearch(list, target), message)
  })
}