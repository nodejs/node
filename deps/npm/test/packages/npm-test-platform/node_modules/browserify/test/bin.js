var test = require('tap').test;
var spawn = require('child_process').spawn;
var path = require('path');
var vm = require('vm');

test('bin', function (t) {
    t.plan(3);
    
    var cwd = process.cwd();
    process.chdir(__dirname);
    
    var ps = spawn(process.execPath, [
        path.resolve(__dirname, '../bin/cmd.js'),
        'entry/main.js',
        '--exports=require'
    ]);
    var src = '';
    ps.stdout.on('data', function (buf) {
        src += buf.toString();
    });
    
    ps.on('exit', function (code) {
        t.equal(code, 0);
        
        var allDone = false;
        var c = {
            done : function () { allDone = true }
        };
        
        vm.runInNewContext(src, c);
        t.deepEqual(
            Object.keys(c.require.modules).sort(),
            [ 'path', '__browserify_process', '/one.js', '/two.js', '/main.js' ].sort()
        );
        t.ok(allDone);
        
        t.end();
    });
});
