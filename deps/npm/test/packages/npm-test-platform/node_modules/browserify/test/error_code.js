var test = require('tap').test;
var spawn = require('child_process').spawn;
var path = require('path');

if (false) require('__32jlkbeep');

test('error code', function (t) {
    t.plan(2);
    
    var cwd = process.cwd();
    process.chdir(__dirname);
    
    var ps = spawn(process.execPath, [
        path.resolve(__dirname, '../bin/cmd.js'),
        path.resolve(__dirname, 'error_code/src.js')
    ]);
    var err = '';
    ps.stderr.on('data', function (buf) { err += buf });
    
    ps.on('exit', function (code) {
        t.notEqual(code, 0);
        t.ok(/^SyntaxError:/m.test(err));
    });
});
