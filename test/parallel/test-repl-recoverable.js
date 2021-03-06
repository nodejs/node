'use strict';

require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

let evalCount = 0;
let recovered = false;
let rendered = false;

function customEval(code, context, file, cb) {
  evalCount++;

  return cb(evalCount === 1 ? new repl.Recoverable() : null, true);
}

const putIn = new ArrayStream();

putIn.write = function(msg) {
  if (msg === '... ') {
    recovered = true;
  }

  if (msg === 'true\n') {
    rendered = true;
  }
};

repl.start('', putIn, customEval);

// https://github.com/nodejs/node/issues/2939
// Expose recoverable errors to the consumer.
putIn.emit('data', '1\n');
putIn.emit('data', '2\n');

process.on('exit', function() {
  assert(recovered, 'REPL never recovered');
  assert(rendered, 'REPL never rendered the result');
  assert.strictEqual(evalCount, 2);
});
