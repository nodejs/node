var path = require('path')
var fs = require('fs')
var spawn = require('child_process').spawn
var t = require('tap')
var node = process.execPath
var wrap = require.resolve('./fixtures/wrap.js')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var fs = require('fs')

if (process.platform === 'win32') {
  t.plan(0, 'No proper shebang support on windows, so skip this')
  process.exit(0)
}

var expect =
  'before in shim\n' +
  'shebang main foo,bar\n' +
  'after in shim\n' +
  'before in shim\n' +
  'shebang main foo,bar\n' +
  'after in shim\n'

var fixdir = path.resolve(__dirname, 'fixtures', 'shebangs')

t.test('setup', function (t) {
  rimraf.sync(fixdir)
  mkdirp.sync(fixdir)
  t.end()
})

t.test('absolute', function (t) {
  var file = path.resolve(fixdir, 'absolute.js')
  runTest(file, process.execPath, t)
})

t.test('env', function (t) {
  var file = path.resolve(fixdir, 'env.js')
  runTest(file, '/usr/bin/env node', t)
})

function runTest (file, shebang, t) {
  var content = '#!' + shebang + '\n' +
    'console.log("shebang main " + process.argv.slice(2))\n'
  fs.writeFileSync(file, content, 'utf8')
  fs.chmodSync(file, '0755')
  var child = spawn(node, [wrap, file, 'foo', 'bar'])
  var out = ''
  var err = ''
  child.stdout.on('data', function (c) {
    out += c
  })
  child.stderr.on('data', function (c) {
    err += c
  })
  child.on('close', function (code,  signal) {
    t.equal(code, 0)
    t.equal(signal, null)
    t.equal(out, expect)
    // console.error(err)
    t.end()
  })
}

t.test('cleanup', function (t) {
  rimraf.sync(fixdir)
  t.end()
})
