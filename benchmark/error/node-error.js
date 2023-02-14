'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  type: ['node', 'regular'],
}, {
  flags: ['--expose-internals'],
});

function main({ n, type }) {
  const {
    codes: {
      ERR_INVALID_STATE,
    },
  } = require('internal/errors');

  const Clazz = type === 'node' ? ERR_INVALID_STATE.TypeError : (() => {
    class Foo extends TypeError {}
    Foo.prototype.constructor = TypeError;
    return Foo;
  })();

  bench.start();
  let length = 0;
  for (let i = 0; i < n; i++) {
    const error = new Clazz('test' + i);
    length += error.name.length;
  }
  bench.end(n);
  assert(length);
}
