'use strict';
const common = require('../common');

// This test ensures that the repl does not
// crash or emit error when throwing `null|undefined`
// ie `throw null` or `throw undefined`

const repl = require('repl');

const replserver = repl.start();
const $eval = replserver.eval;
replserver.eval = function(code, context, file, cb) {
  return $eval.call(this, code, context, file,
                    common.mustNotCall(
                      'repl crashes/throw error on `throw null|undefined`'));
};
replserver.write('throw null\n');
replserver.write('throw undefined\n');

replserver.write('.exit\n');
