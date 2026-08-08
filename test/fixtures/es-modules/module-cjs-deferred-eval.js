// This fixture is imported as a module
// in test/es-module/test-cjs-defer-static-import-eval.mjs
// to ensure a CommonJS module imported with `import defer`
// is only executed once.
const assert = require('assert');

const identifier = 'package-type-commonjs';

module.exports.foo = 42;
module.exports.identifier = identifier;

// The `eval_list` is initialised by the importing module,
// so by the time the fixture is executed, `eval_list` should
// already be initialised.
assert.deepEqual(globalThis.eval_list, []);

globalThis.eval_list.push('defer-1');
