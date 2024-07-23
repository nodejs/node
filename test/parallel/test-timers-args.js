'use strict';
require('../common');
const assert = require('assert');

function range(n) {
  return 'x'.repeat(n + 1).split('').map(function(_, i) { return i; });
}

function timeout(nargs) {
  const args = range(nargs);
  setTimeout.apply(null, [callback].concat(args));

  function callback() {
    assert.deepStrictEqual([].slice.call(arguments), args.slice(1));
    if (nargs < 129) timeout(nargs + 1);
  }
}

function interval(nargs) {
  const args = range(nargs);
  const timer = setTimeout.apply(null, [callback].concat(args));

  function callback() {
    clearInterval(timer);
    assert.deepStrictEqual([].slice.call(arguments), args.slice(1));
    if (nargs < 129) interval(nargs + 1);
  }
}

timeout(0);
interval(0);
