var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var pkg = path.resolve(__dirname, 'shrinkwrap-lifecycle')

test('npm shrinkwrap execution order', function (t) {
  setup()
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Simen Bekkhus',
    name: 'shrinkwrap-lifecycle',
    shrinkwrap: '0.0.0',
    description: 'Test for npm shrinkwrap execution order',
    scripts: {
      preshrinkwrap: 'echo this happens first',
      shrinkwrap: 'echo this happens second',
      postshrinkwrap: 'echo this happens third'
    }
  }), 'utf8')
  common.npm(['shrinkwrap'], [], function (err, code, stdout) {
    if (err) throw err

    var indexOfFirst = stdout.indexOf('echo this happens first')
    var indexOfSecond = stdout.indexOf('echo this happens second')
    var indexOfThird = stdout.indexOf('wrote npm-shrinkwrap.json')
    var indexOfFourth = stdout.indexOf('echo this happens third')

    t.ok(indexOfFirst >= 0)
    t.ok(indexOfSecond >= 0)
    t.ok(indexOfThird >= 0)
    t.ok(indexOfFourth >= 0)

    t.ok(indexOfFirst < indexOfSecond)
    t.ok(indexOfSecond < indexOfThird)
    t.ok(indexOfThird < indexOfFourth)

    t.end()
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  t.end()
})

function setup () {
  mkdirp.sync(pkg)
  process.chdir(pkg)
}
