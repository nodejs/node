var common = require('../common');
var assert = require('assert');
var spawnSync = require('child_process').spawnSync;
var v8 = require('v8');

// --harmony_classes implies --harmony_scoping; ensure that scoping still works
// when classes are disabled.
assert.throws(function() { eval('"use strict"; class C {}'); }, SyntaxError);
eval('"use strict"; let x = 42');  // Should not throw.
eval('"use strict"; const y = 42');  // Should not throw.

v8.setFlagsFromString('--harmony_classes');
eval('"use strict"; class C {}');  // Should not throw.
eval('"use strict"; let x = 42');  // Should not throw.
eval('"use strict"; const y = 42');  // Should not throw.

// Verify that the --harmony_classes flag unlocks classes again.
var args = ['--harmony_classes', '--use_strict', '-p', 'class C {}'];
var cp = spawnSync(process.execPath, args);
assert.equal(cp.status, 0);
assert.equal(cp.stdout.toString('utf8').trim(), '[Function: C]');

// Now do the same for --harmony_object_literals.
assert.throws(function() { eval('({ f() {} })'); }, SyntaxError);
v8.setFlagsFromString('--harmony_object_literals');
eval('({ f() {} })');

var args = ['--harmony_object_literals', '-p', '({ f() {} })'];
var cp = spawnSync(process.execPath, args);
assert.equal(cp.status, 0);
assert.equal(cp.stdout.toString('utf8').trim(), '{ f: [Function: f] }');
