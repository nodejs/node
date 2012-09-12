var browserify = require('../');
var vm = require('vm');
var test = require('tap').test;

test('only export require', function (t) {
    t.plan(1);
    var src = browserify().bundle();
    var c = {};
    vm.runInNewContext(src, c);
    t.same(Object.keys(c), [ 'require' ]);
});

test('no exports when entries are defined', function (t) {
    t.plan(1);
    var src = browserify(__dirname + '/export/entry.js').bundle();
    var c = {};
    vm.runInNewContext(src, c);
    t.same(c, {});
});

test('override require export', function (t) {
    t.plan(1);
    var src = browserify({ exports : [ 'require' ] })
        .addEntry(__dirname + '/export/entry.js')
        .bundle()
    ;
    var c = {};
    vm.runInNewContext(src, c);
    t.same(Object.keys(c), [ 'require' ]);
});

test('override process export', function (t) {
    t.plan(1);
    var src = browserify({ exports : [ 'process' ] })
        .addEntry(__dirname + '/export/entry.js')
        .bundle()
    ;
    var c = {};
    vm.runInNewContext(src, c);
    t.same(Object.keys(c), [ 'process' ]);
});

test('override require and process export', function (t) {
    t.plan(1);
    var src = browserify({ exports : [ 'require', 'process' ] })
        .addEntry(__dirname + '/export/entry.js')
        .bundle()
    ;
    var c = {};
    vm.runInNewContext(src, c);
    t.same(Object.keys(c).sort(), [ 'require', 'process' ].sort());
});
