var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'shrinkwrap-dev-dependency')

var opts = [
  '--cache', path.resolve(pkg, 'cache'),
  '--registry', common.registry
]

var desired = {
  name: 'npm-test-shrinkwrap-dev-dependency',
  version: '0.0.0',
  dependencies: {
    request: {
      version: '0.9.0',
      from: 'request@0.9.0',
      resolved: common.registry + '/request/-/request-0.9.0.tgz'
    },
    underscore: {
      version: '1.3.1',
      from: 'underscore@1.3.1',
      resolved: common.registry + '/underscore/-/underscore-1.3.1.tgz'
    }
  }
}

var json = {
  author: 'Domenic Denicola',
  name: 'npm-test-shrinkwrap-dev-dependency',
  version: '0.0.0',
  dependencies: {
    request: '0.9.0',
    underscore: '1.3.1'
  },
  devDependencies: {
    underscore: '1.5.1'
  }
}

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2))
  process.chdir(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

test('setup', function (t) {
  setup()
  t.end()
})

test("shrinkwrap doesn't strip out the dependency", function (t) {
  t.plan(3)
  setup()

  mr({port: common.port}, function (er, s) {
    common.npm(opts.concat(['install', '.']), {stdio: [0, 'pipe', 2]}, function (err, code) {
      if (err) throw err
      if (!t.is(code, 0)) return (s.close(), t.end())
      common.npm(opts.concat(['shrinkwrap']), {stdio: [0, 2, 2]}, function (err, code) {
        if (err) throw err
        t.is(code, 0)
        try {
          var results = JSON.parse(fs.readFileSync(path.join(pkg, 'npm-shrinkwrap.json')))
        } catch (ex) {
          t.comment(ex)
        }
        t.deepEqual(results, desired)
        s.close()
        t.end()
      })
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
