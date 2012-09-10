var browserify = require('../');
var vm = require('vm');
var test = require('tap').test;

test('subdep', function (t) {
    t.plan(1);
    
    var src = browserify.bundle({ require : __dirname + '/subdep/index.js' });
    var c = {};
    vm.runInNewContext(src, c);
    t.deepEqual(
        Object.keys(c.require.modules).sort(),
        [
            '/package.json',
            '/index.js',
            '/node_modules/qq/package.json',
            '/node_modules/qq/b.js',
            '/node_modules/qq/node_modules/a/package.json',
            '/node_modules/qq/node_modules/a/index.js',
            '/node_modules/qq/node_modules/z/index.js',
            'path',
            '__browserify_process'
        ].sort()
    );
    t.end();
});
