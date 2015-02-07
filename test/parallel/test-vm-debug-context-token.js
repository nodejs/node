var common = require('../common');
var vm = require('vm');
var assert = require('assert');

var proto = vm.runInDebugContext('DebugCommandProcessor.prototype');
proto.prop = true;
assert.equal(proto.prop, true);
proto = vm.runInDebugContext('DebugCommandProcessor.prototype');
assert.equal(proto.prop, true);
proto = vm.runInDebugContext('DebugCommandProcessor.prototype.prop = true;' +
                             'DebugCommandProcessor.prototype');
assert.equal(proto.prop, true);
