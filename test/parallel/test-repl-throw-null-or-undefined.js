'use strict';
require('../common');

// This test ensures that the repl does not
// crash or emit error when throwing `null|undefined`
// ie `throw null` or `throw undefined`

const assert = require('assert');
const repl = require('repl');

const r = repl.start();

assert.doesNotThrow(() => {
  r.write('throw null\n');
  r.write('throw undefined\n');
}, TypeError, 'repl crashes/throw error on `throw null|undefined`');

r.write('.exit\n');
