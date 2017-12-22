'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

let recovered = false;
let rendered = false;

function customEval(code, context, file, cb) {
  return cb(!recovered ? new repl.Recoverable() : null, true);
}

const putIn = new common.ArrayStream();

putIn.write = function(msg) {
  if (msg === '... ') {
    recovered = true;
  }

  if (msg === 'true\n') {
    rendered = true;
  }
};

repl.start('', putIn, common.mustCall(customEval, 2));

// https://github.com/nodejs/node/issues/2939
// Expose recoverable errors to the consumer.
putIn.emit('data', '1\n');
putIn.emit('data', '2\n');

process.on('exit', function() {
  assert(recovered, 'REPL never recovered');
  assert(rendered, 'REPL never rendered the result');
});
