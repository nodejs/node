var test = require('tap').test
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var spawn = require('child_process').spawn
var npm = require.resolve('../../bin/npm-cli.js')
var node = process.execPath
var pkg = path.resolve(__dirname, 'sorted-package-json')
var tmp = path.join(pkg, 'tmp')
var cache = path.join(pkg, 'cache')
var fs = require('fs')
var common = require('../common-tap.js')
var mr = require('npm-registry-mock')
var osenv = require('osenv')

test('sorting dependencies', function (t) {
  var packageJson = path.resolve(pkg, 'package.json')

  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  setup()

  var before = JSON.parse(fs.readFileSync(packageJson).toString())

  mr({ port: common.port }, function (er, s) {
    // underscore is already in the package.json,
    // but --save will trigger a rewrite with sort
    var child = spawn(node, [npm, 'install', '--save', 'underscore@1.3.3', '--no-progress', '--loglevel=error'], {
      cwd: pkg,
      env: {
        'npm_config_registry': common.registry,
        'npm_config_cache': cache,
        'npm_config_tmp': tmp,
        'npm_config_prefix': pkg,
        'npm_config_global': 'false',
        HOME: process.env.HOME,
        Path: process.env.PATH,
        PATH: process.env.PATH
      },
      stdio: ['ignore', 'ignore', process.stderr]
    })

    child.on('close', function (code) {
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
  mkdirp.sync(pkg)

  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
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
}
