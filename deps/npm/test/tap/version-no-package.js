var common = require('../common-tap.js')
var test = require('tap').test

var pkg = common.pkg

test('npm version in a prefix with no package.json', function (t) {
  process.chdir(pkg)
  common.npm(
    ['version', '--json', '--prefix', pkg],
    { cwd: pkg, nodeExecPath: process.execPath },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm version doesn't care that there's no package.json")
      t.notOk(code, 'npm version ran without barfing')
      t.ok(stdout, 'got version output')
      t.notOk(stderr, 'no error output')
      t.doesNotThrow(function () {
        var metadata = JSON.parse(stdout)
        t.equal(metadata.node, process.versions.node, 'node versions match')
      }, 'able to reconstitute version object from stdout')
      t.end()
    }
  )
})
