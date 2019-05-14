var common = require('../common-tap.js')
var t = require('tap')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var path = require('path')
var dir = path.join(__dirname, 'my_node_modules')
var script = process.platform === 'win32' ? 'echo %PATH%' : 'echo $PATH'

t.test('setup', function (t) {
  rimraf.sync(dir)
  mkdirp.sync(dir)
  fs.writeFileSync(dir + '/package.json', JSON.stringify({
    name: 'my_node_modules',
    version: '1.2.3',
    scripts: {
      test: script
    }
  }))
  t.end()
})

t.test('verify PATH is munged right', function (t) {
  common.npm(['test'], { cwd: dir }, function (err, code, stdout, stderr) {
    if (err) {
      throw err
    }
    t.equal(code, 0, 'exit ok')
    t.notOk(stderr, 'should have no stderr')
    var expect = path.resolve(dir, 'node_modules', '.bin').toLowerCase()
    t.contains(stdout.toLowerCase(), expect)
    t.end()
  })
})

t.test('cleanup', function (t) {
  rimraf.sync(dir)
  t.end()
})
