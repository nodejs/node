var test = require('tap').test;
var spawn = require('child_process').spawn;
var path = require('path');
var vm = require('vm');

test('retarget with -r', function (t) {
    t.plan(2);
    
    var cwd = process.cwd();
    process.chdir(__dirname);
    
    var ps = spawn(process.execPath, [
        path.resolve(__dirname, '../bin/cmd.js'),
        '-r', 'beep',
        '--exports=require'
    ]);
    var src = '';
    ps.stdout.on('data', function (buf) { src += buf });
    
    ps.on('exit', function (code) {
        t.equal(code, 0);
        
        var c = {};
        vm.runInNewContext(src, c);
        t.equal(c.require('beep'), 'boop');
    });
});
