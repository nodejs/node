var test = require('tap').test
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var common = require('../common-tap.js')
var pkg = common.pkg
var tmp = path.join(pkg, 'tmp')
var cache = path.join(pkg, 'cache')
var fs = require('fs')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var packageJson = path.resolve(pkg, 'package.json')

test('setup', function (t) {
  setup()
  t.pass('setup success')
  t.done()
})

test('sorting dependencies', function (t) {
  var before = JSON.parse(fs.readFileSync(packageJson).toString())

  mr({ port: common.port }, function (er, s) {
    // underscore is already in the package.json,
    // but --save will trigger a rewrite with sort
    common.npm([
      'install',
      '--save', 'underscore@1.3.3',
      '--no-progress',
      '--cache', cache,
      '--tmp', tmp,
      '--registry', common.registry
    ], {
      cwd: pkg
    }, function (err, code, stdout, stderr) {
      t.ifError(err, 'no error')
      t.equal(code, 0, 'npm install exited with code')
      var result = fs.readFileSync(packageJson).toString()
      var resultAsJson = JSON.parse(result)

      s.close()

      t.same(Object.keys(resultAsJson.dependencies),
        Object.keys(before.dependencies).sort())

      t.notSame(Object.keys(resultAsJson.dependencies),
        Object.keys(before.dependencies))

      t.ok(resultAsJson.dependencies.underscore)
      t.ok(resultAsJson.dependencies.request)
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)

  fs.writeFileSync(packageJson, JSON.stringify({
    'name': 'sorted-package-json',
    'version': '0.0.0',
    'description': '',
    'main': 'index.js',
    'scripts': {
      'test': 'echo \'Error: no test specified\' && exit 1'
    },
    'author': 'Rocko Artischocko',
    'license': 'ISC',
    'dependencies': {
      'underscore': '^1.3.3',
      'request': '^0.9.0'
    }
  }, null, 2), 'utf8')
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(cache)
  rimraf.sync(pkg)
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
}
