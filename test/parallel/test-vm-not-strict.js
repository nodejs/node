/* eslint-disable strict, no-var, no-delete-var, node-core/required-modules, node-core/require-common-first */
// While common, should be used as first require of the test, using it causes errors: Unexpected global(s) found: data, b.
// Actually in this specific test case calling vm.runInThisContext exposes variables outside of the vm (behaviour existing since at least v14).
// The other rules (strict, no-var, no-delete-var) have been disabled to test this specific not strict case.
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

// While the variables have been declared in the vm context, they are accessible in the global one too.
// This behaviour has been there at least from v14 of node, and still exist in 16 and 18.
assert.equal(typeof unusedB, 'number');
assert.equal(typeof unusedC, 'number');
