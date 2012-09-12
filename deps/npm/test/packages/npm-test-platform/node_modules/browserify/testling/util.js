var test = require('testling');
var util = require('./builtins/util');
var EventEmitter = require('events').EventEmitter;

test('util.inspect', function (t) {
    t.plan(1);
    t.equal(util.inspect([1,2,3]), '[ 1, 2, 3 ]');
    t.end();
});

test('util.inherits', function (t) {
    t.plan(2);
    
    function Beep () {}
    util.inherits(Beep, EventEmitter);
    var beep = new Beep;
    
    t.ok(beep instanceof Beep, 'is a Beep');
    t.ok(
        (beep instanceof EventEmitter) || typeof beep.emit === 'function',
        'is an EventEmitter'
    );
    t.end();
});
