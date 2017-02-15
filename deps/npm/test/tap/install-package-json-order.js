var test = require('tap').test
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var spawn = require('child_process').spawn
var npm = require.resolve('../../bin/npm-cli.js')
var node = process.execPath
var pkg = path.resolve(__dirname, 'install-package-json-order')
var workdir = path.join(pkg, 'workdir')
var tmp = path.join(pkg, 'tmp')
var cache = path.join(pkg, 'cache')
var fs = require('fs')
var osenv = require('osenv')

test('package.json sorting after install', function (t) {
  var packageJson = path.resolve(pkg, 'package.json')
  var installedPackage = path.resolve(workdir,
    'node_modules/install-package-json-order/package.json')

  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  mkdirp.sync(workdir)
  setup()

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

test('cleanup', function (t) {
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function setup () {
  mkdirp.sync(pkg)

  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    'name': 'install-package-json-order',
    'version': '0.0.0',
    'array': [ 'one', 'two', 'three' ]
  }, null, 2), 'utf8')
  fs.writeFileSync(path.resolve(workdir, 'package.json'), JSON.stringify({
    'name': 'install-package-json-order-work',
    'version': '0.0.0'
  }, null, 2), 'utf8')
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(cache)
  rimraf.sync(pkg)
}
