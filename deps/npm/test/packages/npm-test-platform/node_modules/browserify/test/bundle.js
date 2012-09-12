var browserify = require('../');
var vm = require('vm');
var test = require('tap').test;

test('bundle', function (t) {
    var b = browserify();
    b.require('seq');
    var src = b.bundle();
    
    t.plan(3);
    
    t.ok(typeof src === 'string');
    t.ok(src.length > 0);
    
    var c = {
        setTimeout : setTimeout,
        console : console
    };
    vm.runInNewContext(src, c);
    
    c.require('seq')([1,2,3])
        .parMap_(function (next, x) {
            setTimeout(function () {
                next.ok(x * 100)
            }, 10)
        })
        .seq(function (x,y,z) {
            t.deepEqual([x,y,z], [100,200,300]);
            t.end();
        })
    ;
});
