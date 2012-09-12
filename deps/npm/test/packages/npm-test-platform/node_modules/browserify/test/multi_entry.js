var browserify = require('../');
var vm = require('vm');
var test = require('tap').test;

test('multi entry', function (t) {
    t.plan(3);
    
    var src = browserify(__dirname + '/multi_entry/a.js', {
        entry : [
            __dirname + '/multi_entry/b.js',
            __dirname + '/multi_entry/c.js'
        ]
    }).bundle();
    
    var c = {
        times : 0,
        t : t
    };
    vm.runInNewContext(src, c);
});
