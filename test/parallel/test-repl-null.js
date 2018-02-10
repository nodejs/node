'use strict';
const common = require('../common');
const repl = require('repl');
const assert = require('assert');

const callbacks = [
  common.mustCall((err, value) => {
    assert.ifError(err);
    assert.strictEqual(value, undefined);
  })
];
const replserver = new repl.REPLServer();
const $eval = replserver.eval;
replserver.eval = function(code, context, file, cb) {
  const expected = callbacks.shift();
  return $eval.call(this, code, context, file, (...args) => {
    try {
      expected(...args);
    } catch (e) {
      console.error(e);
      process.exit(1);
    }
    cb(...args);
  });
};

replserver._inTemplateLiteral = true;

// `null` gets treated like an empty string. (Should it? You have to do some
// strange business to get it into the REPL. Maybe it should really throw?)

assert.doesNotThrow(() => {
  replserver.emit('line', null);
});

replserver.emit('line', '.exit');
