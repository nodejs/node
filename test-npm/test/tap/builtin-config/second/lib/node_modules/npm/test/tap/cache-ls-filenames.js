var t = require('tap')
var path = require('path')
var fs = require('fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var spawn = require('child_process').spawn
var npm = require.resolve('../../bin/npm-cli.js')
var dir = path.resolve(__dirname, 'cache-ls-filenames')
var node = process.execPath

t.test('setup', function (t) {
  rimraf.sync(dir)
  mkdirp.sync(dir + '/a/b/c/d')
  for (var i = 0; i < 5; i++) {
    fs.writeFileSync(dir + '/file-' + i, 'x\n')
    fs.writeFileSync(dir + '/a/b/file-' + i, 'x\n')
  }
  t.end()
})

function test (t, args) {
  var child = spawn(node, [npm, 'cache', 'ls', '--cache=' + dir].concat(args))
  var out = ''
  child.stdout.on('data', function (c) {
    out += c
  })
  child.on('close', function (code, signal) {
    t.equal(code, 0)
    t.equal(signal, null)
    out.trim().split(/[\n\r]+/).map(function (filename) {
      return filename.replace(/^~/, process.env.HOME)
    }).forEach(function (file) {
      // verify that all exist
      t.ok(fs.existsSync(file), 'exists: ' + file)
    })
    t.end()
  })
}

t.test('without path arg', function (t) {
  test(t, [])
})

t.test('with path arg', function (t) {
  test(t, ['a'])
})

t.test('cleanup', function (t) {
  rimraf.sync(dir)
  t.end()
})
