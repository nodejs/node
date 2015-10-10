// if 'npm init' is interrupted with ^C, don't report
// 'init written successfully'
var test = require('tap').test
var path = require('path')
var osenv = require('osenv')
var rimraf = require('rimraf')
var npmlog = require('npmlog')
var requireInject = require('require-inject')

var npm = require('../../lib/npm.js')

var PKG_DIR = path.resolve(__dirname, 'init-interrupt')

test('setup', function (t) {
  cleanup()

  t.end()
})

test('issue #6684 remove confusing message', function (t) {

  var initJsonMock = function (dir, input, config, cb) {
    process.nextTick(function () {
      cb({ message: 'canceled' })
    })
  }
  initJsonMock.yes = function () { return true }

  npm.load({ loglevel: 'silent' }, function () {
    var log = ''
    var init = requireInject('../../lib/init', {
      'init-package-json': initJsonMock
    })

    // capture log messages
    npmlog.on('log', function (chunk) { log += chunk.message + '\n' })

    init([], function (err, code) {
      t.ifError(err, 'init ran successfully')
      t.notOk(code, 'exited without issue')
      t.notSimilar(log, /written successfully/, 'no success message written')
      t.similar(log, /canceled/, 'alerted that init was canceled')

      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()

  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(PKG_DIR)
}
