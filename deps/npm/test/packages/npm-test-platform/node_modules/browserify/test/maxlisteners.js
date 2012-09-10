var EventEmitter = require('../builtins/events').EventEmitter;
var test = require('tap').test;

test('setMaxListener', function (t) {
    var ee = new EventEmitter;
    ee.setMaxListeners(5);
    t.end();
});
