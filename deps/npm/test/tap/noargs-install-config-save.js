var common = require('../common-tap.js')
var test = require('tap').test
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')

var mr = require('npm-registry-mock')

var pkg = common.pkg

function writePackageJson () {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  mkdirp.sync(pkg + '/cache')

  fs.writeFileSync(pkg + '/package.json', JSON.stringify({
    'author': 'Rocko Artischocko',
    'name': 'noargs',
    'version': '0.0.0',
    'devDependencies': {
      'underscore': '1.3.1'
    }
  }), 'utf8')
}

var env = {
  'npm_config_save': true,
  'npm_config_registry': common.registry,
  'npm_config_cache': pkg + '/cache',
  HOME: process.env.HOME,
  Path: process.env.PATH,
  PATH: process.env.PATH
}
var OPTS = {
  cwd: pkg,
  env: env
}

test('does not update the package.json with empty arguments', function (t) {
  writePackageJson()
  t.plan(2)

  mr({ port: common.port }, function (er, s) {
    common.npm('install', OPTS, function (er, code, stdout, stderr) {
      if (er) throw er
      t.is(code, 0)
      if (code !== 0) {
        console.error('#', stdout)
        console.error('#', stderr)
      }
      var text = JSON.stringify(fs.readFileSync(pkg + '/package.json', 'utf8'))
      s.close()
      t.equal(text.indexOf('"dependencies'), -1, 'dependencies do not exist in file')
    })
  })
})

test('updates the package.json (adds dependencies) with an argument', function (t) {
  writePackageJson()
  t.plan(2)

  mr({ port: common.port }, function (er, s) {
    common.npm(['install', 'underscore', '-P'], OPTS, function (er, code, stdout, stderr) {
      if (er) throw er
      t.is(code, 0)
      s.close()
      var text = JSON.stringify(fs.readFileSync(pkg + '/package.json', 'utf8'))
      t.notEqual(text.indexOf('"dependencies'), -1, 'dependencies exist in file')
    })
  })
})

test('cleanup', function (t) {
  rimraf.sync(pkg)
  t.end()
})
