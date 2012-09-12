var browserify = require('../');
var test = require('tap').test;

test('multi ignore', function (t) {
    
    var bundle = browserify(__dirname + '/multi_entry/a.js', {
        ignore : [
            __dirname + '/multi_entry/b.js'
        ]
    });
    t.ok(bundle.ignoring[__dirname + '/multi_entry/b.js']);

    bundle.ignore(__dirname + '/multi_entry/c.js');
    t.ok(bundle.ignoring[__dirname + '/multi_entry/b.js']);
    t.ok(bundle.ignoring[__dirname + '/multi_entry/c.js']);
    
    bundle.ignore(__dirname + '/multi_entry/d.js');
    t.ok(bundle.ignoring[__dirname + '/multi_entry/b.js']);
    t.ok(bundle.ignoring[__dirname + '/multi_entry/c.js']);
    t.ok(bundle.ignoring[__dirname + '/multi_entry/d.js']);
    
    t.end();
});
