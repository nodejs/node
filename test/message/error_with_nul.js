'use strict';
require('../common');

function test() {
  const a = 'abc\0def';
  console.error(a);
  throw new Error(a);
}
Object.defineProperty(test, 'name', { value: 'fun\0name' });

test();
