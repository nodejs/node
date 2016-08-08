'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

let evalCount = 0;
let recovered = false;
let rendered = false;

function customEval(code, context, file, cb) {
  evalCount++;

  return cb(evalCount === 1 ? new repl.Recoverable() : null, true);
}

const putIn = new common.ArrayStream();

putIn.write = function(msg) {
  if (msg === '... ') {
    recovered = true;
  }

  if (msg === 'true\r\n') {
    rendered = true;
  }
};

repl.start('', putIn, customEval);

// https://github.com/nodejs/node/issues/2939
// Expose recoverable errors to the consumer.
putIn.emit('data', '1\r\n');
putIn.emit('data', '2\r\n');

process.on('exit', function() {
  assert(recovered, 'REPL never recovered');
  assert(rendered, 'REPL never rendered the result');
  assert.strictEqual(evalCount, 2);
});
