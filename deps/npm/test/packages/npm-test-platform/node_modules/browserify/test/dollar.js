var browserify = require('../');
var vm = require('vm');
var test = require('tap').test;

test('dollar', function (t) {
    t.plan(3);
    var src = browserify({
        require : __dirname + '/dollar/dollar/index.js'
    }).bundle();
    
    t.ok(typeof src === 'string');
    t.ok(src.length > 0);
    
    var c = {};
    vm.runInNewContext(src, c);
    var res = vm.runInNewContext('require("./")(100)', c);
    t.equal(res, 10000);
    t.end();
});
