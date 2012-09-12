var assert = require('assert');
var browserify = require('../');
var vm = require('vm');
var test = require('tap').test;

test('fieldString', function (t) {
    t.plan(1);
    
    var dir = __dirname + '/field/';
    var src = browserify({ require : dir + '/string.js' }).bundle();
    
    var c = {};
    vm.runInNewContext(src, c);
    t.equal(
        c.require('./string.js'),
        'browser'
    );
    t.end();
});

test('fieldObject', function (t) {
    t.plan(1);
    
    var dir = __dirname + '/field/';
    var src = browserify({ require : dir + '/object.js' }).bundle();
    
    var c = {};
    vm.runInNewContext(src, c);
    t.equal(
        c.require('./object.js'),
        'browser'
    );
    t.end();
});

test('missObject', function (t) {
    t.plan(1);
    
    var dir = __dirname + '/field/';
    var src = browserify({ require : dir + '/miss.js' }).bundle();
    
    var c = {};
    vm.runInNewContext(src, c);
    t.equal(
        c.require('./miss.js'),
        '!browser'
    );
    t.end();
});

test('fieldSub', function (t) {
    t.plan(1);
    
    var dir = __dirname + '/field/';
    var src = browserify({ require : dir + '/sub.js' }).bundle();
    
    var c = {};
    vm.runInNewContext(src, c);
    t.equal(
        c.require('./sub.js'),
        'browser'
    );
    t.end();
});
