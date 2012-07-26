var test = require('tap').test;
var promzard = require('../');
var fs = require('fs')
var file = __dirname + '/fn.input';

var expect = {
  a : 3,
  b : '!2B...',
  c : {
    x : 5500,
    y : '/tmp/y/file.txt',
  }
}
expect.a_function = fs.readFileSync(file, 'utf8')
expect.asyncPrompt = 'async prompt'

if (process.argv[2] === 'child') {
  return child()
}

test('prompt callback param', function (t) {
  t.plan(1);

  var spawn = require('child_process').spawn
  var child = spawn(process.execPath, [__filename, 'child'])

  var output = ''
  child.stderr.on('data', function (c) {
    output += c
  })

  child.on('close', function () {
    console.error('output=%j', output)
    output = JSON.parse(output)
    t.same(output, expect);
    t.end()
  })

  setTimeout(function () {
    child.stdin.write('\n')
  }, 100)
  setTimeout(function () {
    child.stdin.write('55\n')
  }, 150)
  setTimeout(function () {
    child.stdin.write('async prompt\n')
  }, 200)
})

function child () {
  var ctx = { tmpdir : '/tmp' }
  var file = __dirname + '/fn.input';
  promzard(file, ctx, function (err, output) {
    console.error(JSON.stringify(output))
  })
}
