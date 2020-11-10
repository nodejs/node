var test = require('tap').test
var path = require('path')
var mkdirp = require('mkdirp')
var spawn = require('child_process').spawn
var npm = require.resolve('../../bin/npm-cli.js')
var node = process.execPath
const common = require('../common-tap.js')
var pkg = common.pkg
var workdir = path.join(pkg, 'workdir')
var tmp = path.join(pkg, 'tmp')
var fs = require('fs')

test('package.json sorting after install', function (t) {
  var packageJson = path.resolve(pkg, 'package.json')
  var installedPackage = path.resolve(workdir,
    'node_modules/install-package-json-order/package.json')

  mkdirp.sync(tmp)
  mkdirp.sync(workdir)

  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    'name': 'install-package-json-order',
    'version': '0.0.0',
    'array': [ 'one', 'two', 'three' ]
  }, null, 2), 'utf8')

  fs.writeFileSync(path.resolve(workdir, 'package.json'), JSON.stringify({
    'name': 'install-package-json-order-work',
    'version': '0.0.0'
  }, null, 2), 'utf8')

  var before = JSON.parse(fs.readFileSync(packageJson).toString())
  var child = spawn(node, [npm, 'install', pkg], { cwd: workdir })

  child.on('close', function (code) {
    t.equal(code, 0, 'npm install exited with code')
    var result = fs.readFileSync(installedPackage, 'utf8')
    var resultAsJson = JSON.parse(result)
    t.same(resultAsJson.array, before.array)
    t.end()
  })
})
