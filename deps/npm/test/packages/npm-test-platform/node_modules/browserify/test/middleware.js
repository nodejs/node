var fs = require('fs');
var vm = require('vm');
var browserify = require('../');
var test = require('tap').test;

test('middleware', function (t) {
    t.plan(1);
    var bundle = browserify().use(function (b) {
        b.files.middleware = {
            target : '/node_modules/middleware/index.js',
            body : 'module.exports = 555'
        };
    });
    
    bundle.require('middleware');
    
    var src = bundle.bundle();
    var c = { console : console };
    vm.runInNewContext(src, c);
    
    t.equal(c.require('middleware'), 555);
    t.end();
});

test('fn', function (t) {
    t.plan(2);
    var b = browserify().append('doom');
    t.equal(typeof b, 'function');
    t.equal(typeof b.bundle, 'function');
    t.end();
});
