var vm = require('vm');
var fs = require('fs');
var browserify = require('../');
var test = require('tap').test;

test('coffee', function (t) {
    t.plan(8);
    
    var src = browserify.bundle({ require : __dirname + '/coffee/index.coffee' });
    var c = {};
    vm.runInNewContext(src, c);
    
    t.equal(c.require('./foo.coffee')(5), 50);
    t.equal(c.require('./foo')(5), 50);
    
    t.equal(c.require('./bar.js'), 500);
    t.equal(c.require('./bar'), 500);
    
    t.equal(c.require('./baz.coffee'), 1000);
    t.equal(c.require('./baz'), 1000);
    
    t.equal(c.require('./'), 10 * 10 * 500 + 1000);
    t.equal(c.require('./index.coffee'), 10 * 10 * 500 + 1000);
    t.end();
});

test('coffeeEntry', function (t) {
    var b = browserify({ entry : __dirname + '/coffee/entry.coffee' });
    var src = b.bundle();
    
    var c = {
        setTimeout : setTimeout,
        done : function (fn) {
            t.equal(fn(10), 100);
            t.end();
        }
    };
    vm.runInNewContext(src, c);
});
