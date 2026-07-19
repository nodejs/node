/* eslint-disable strict, no-var, no-delete-var, no-undef, node-core/required-modules, node-core/require-common-first */
// Importing common would break the execution. Indeed running `vm.runInThisContext` alters the global context
// when declaring new variables with `var`. The other rules (strict, no-var, no-delete-var) have been disabled
// in order to be able to test this specific not-strict case playing with `var` and `delete`.
// Related to bug report: https://github.com/nodejs/node/issues/43129
var assert = require('assert');
var vm = require('vm');

var data = [];
var a = 'direct';
delete a;
data.push(a);

var item2 = vm.runInThisContext(`
var unusedB = 1;
var data = [];
var b = "this";
delete b;
data.push(b);
data[0]
`);
data.push(item2);

vm.runInContext(
  `
var unusedC = 1;
var c = "new";
delete c;
data.push(c);
`,
  vm.createContext({ data: data })
);

assert.deepStrictEqual(data, ['direct', 'this', 'new']);

assert.strictEqual(typeof unusedB, 'number'); // Declared within runInThisContext
assert.strictEqual(typeof unusedC, 'undefined'); // Declared within runInContext
