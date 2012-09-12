var browserify = require('../');
var test = require('tap').test;
var util = require('util');
var vm = require('vm');

test('util.inspect', function (t) {
    t.plan(1);
    var src = browserify().require('util').bundle();
    var c = {};
    vm.runInNewContext(src, c);
    t.equal(
        c.require('util').inspect([1,2,3]),
        util.inspect([1,2,3])
    );
    t.end();
});

test('util.inherits', function (t) {
    t.plan(2);
    var src = browserify().require('util').require('events').bundle();
    var c = {};
    vm.runInNewContext(src, c);
    var EE = c.require('events').EventEmitter;
    
    function Beep () {}
    c.require('util').inherits(Beep, EE);
    var beep = new Beep;
    
    t.ok(beep instanceof Beep);
    t.ok(beep instanceof EE);
    t.end();
});

test('util.inherits without Object.create', function (t) {
    t.plan(2);
    var src = browserify().require('util').require('events').bundle();
    var c = { Object : {} };
    vm.runInNewContext(src, c);
    var EE = c.require('events').EventEmitter;
    
    function Beep () {}
    c.require('util').inherits(Beep, EE);
    var beep = new Beep;
    
    t.ok(beep instanceof Beep);
    t.ok(beep instanceof EE);
    t.end();
});
