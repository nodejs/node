var fs = require('fs')
var path = require('path')

var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var common = require('../common-tap.js')

var pkg = common.pkg

test('setup', function (t) {
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify({
      name: 'publish-access',
      version: '1.2.5'
    }))
  t.pass('setup done')
  t.end()
})

test('unscoped packages cannot be restricted', function (t) {
  var args = ['--access=restricted', '--loglevel=warn', '--registry=' + common.registry]
  var opts = {stdio: [0, 1, 'pipe'], cwd: pkg}
  common.npm(['publish'].concat(args), opts, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notEqual(code, 0, 'publish not successful')
    t.match(stderr, "Can't restrict access to unscoped packages.")

    t.end()
  })
})

test('cleanup', function (t) {
  rimraf.sync(pkg)
  t.end()
})
