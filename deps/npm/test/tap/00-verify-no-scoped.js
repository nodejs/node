'use strict'
var common = require('../common-tap')
var test = require('tap').test
var path = require('path')
var cwd = path.resolve(__dirname, '..', '..')
var fs = require('fs')

/*
We can't include any scoped modules in bundled dependencies due to a bug in
npm@<4.4.3 that made any module that bundled scoped dependencies
uninstallable. While this is fixed, we can't have them in ourselves without
making it impossible to upgrade, thus this test.
*/

test('no scoped transitive deps', function (t) {
  t.ok(fs.existsSync(cwd), 'ensure that the path we are calling ls within exists')

  var opt = { cwd: cwd, stdio: [ 'ignore', 'pipe', 2 ] }
  common.npm(['ls', '--parseable', '--production'], opt, function (err, code, stdout) {
    t.ifError(err, 'error should not exist')
    t.equal(code, 0, 'npm ls exited with code')
    var matchScoped = new RegExp(path.join(cwd, 'node_modules', '.*@').replace(/\\/g, '\\\\'))
    stdout.split(/\n/).forEach(function (line) {
      if (matchScoped.test(line)) {
        t.notLike(line, matchScoped, 'prod deps do not contain scoped modules')
      }
    })
    t.end()
  })
})
