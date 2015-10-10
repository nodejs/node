var common = require('../common-tap')
var test = require('tap').test
var path = require('path')
var cwd = path.resolve(__dirname, '..', '..')
var fs = require('fs')

test('npm ls in npm', function (t) {
  t.ok(fs.existsSync(cwd), 'ensure that the path we are calling ls within exists')
  var files = fs.readdirSync(cwd)
  t.notEqual(files.length, 0, 'ensure there are files in the directory we are to ls')

  var opt = { cwd: cwd, stdio: [ 'ignore', 'ignore', 2 ] }
  common.npm(['ls'], opt, function (err, code) {
    t.ifError(err, 'error should not exist')
    t.equal(code, 0, 'npm ls exited with code')
    t.end()
  })
})
