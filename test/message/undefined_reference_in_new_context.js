var common = require('../common');
var assert = require('assert');

common.error('before');

var Script = process.binding('evals').Script;

// undefined reference
script = new Script('foo.bar = 5;');
script.runInNewContext();

common.error('after');
