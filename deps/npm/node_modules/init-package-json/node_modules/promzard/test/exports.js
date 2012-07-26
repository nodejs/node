var test = require('tap').test;
var promzard = require('../');

if (process.argv[2] === 'child') {
  return child()
}

test('exports', function (t) {
  t.plan(1);

  var spawn = require('child_process').spawn
  var child = spawn(process.execPath, [__filename, 'child'])

  var output = ''
  child.stderr.on('data', function (c) {
    output += c
  })

  setTimeout(function () {
    child.stdin.write('\n');
  }, 100)
  setTimeout(function () {
    child.stdin.write('55\n');
  }, 200)

  child.on('close', function () {
    console.error('output=%j', output)
    output = JSON.parse(output)
    t.same({
      a : 3,
      b : '!2b',
      c : {
        x : 55,
        y : '/tmp/y/file.txt',
      }
    }, output);
    t.end()
  })
});

function child () {
  var ctx = { tmpdir : '/tmp' }
  var file = __dirname + '/exports.input';

  promzard(file, ctx, function (err, output) {
    console.error(JSON.stringify(output))
  });
}
