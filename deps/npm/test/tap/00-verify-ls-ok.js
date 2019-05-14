var common = require('../common-tap')
var test = require('tap').test
var path = require('path')
var cwd = path.resolve(__dirname, '..', '..')
var fs = require('fs')

test('npm ls in npm', function (t) {
  t.ok(fs.existsSync(cwd), 'ensure that the path we are calling ls within exists')
  var files = fs.readdirSync(cwd)
  t.notEqual(files.length, 0, 'ensure there are files in the directory we are to ls')

  var opt = { cwd: cwd, stdio: [ 'ignore', 'pipe', 2 ] }
  common.npm(['ls', '--json'], opt, function (err, code, stdout) {
    t.ifError(err, 'error should not exist')
    t.equal(code, 0, 'npm ls exited with code')
    const tree = JSON.parse(stdout).dependencies
    // We need to have a toplevel `node-gyp` available, but we also need to
    // make sure npm-lifecycle's version is updated in concert.
    // See https://github.com/npm/npm/issues/20163
    t.deepEqual(
      tree['npm-lifecycle'].dependencies['node-gyp'].version,
      tree['node-gyp'].version,
      'npm-lifecycle and npm using same version of node-gyp'
    )
    t.end()
  })
})
