var test = require('tap').test;
var promzard = require('../');
var fs = require('fs')

test('prompt callback param', function (t) {
    t.plan(1);

    var ctx = { tmpdir : '/tmp' }
    var file = __dirname + '/fn.input';
    promzard(file, ctx, function (err, output) {
        var expect = 
            {
                a : 3,
                b : '!2B...',
                c : {
                    x : 5500,
                    y : '/tmp/y/file.txt',
                }
            }
        expect.a_function = fs.readFileSync(file, 'utf8')
        expect.asyncPrompt = 'async prompt'
        t.same(
            output,
            expect
        );
    });

    setTimeout(function () {
        process.stdin.emit('data', '\n');
    }, 100);

    setTimeout(function () {
        process.stdin.emit('data', '55\n');
    }, 200);

    setTimeout(function () {
        process.stdin.emit('data', 'async prompt')
    }, 300)
});
