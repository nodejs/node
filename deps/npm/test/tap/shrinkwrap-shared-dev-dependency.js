var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var opts = {
  env: common.newEnv().extend({
    npm_config_cache: path.resolve(pkg, 'cache'),
    npm_config_registry: common.registry
  }),
  stdio: [0, 1, 2],
  cwd: pkg
}

var json = {
  author: 'Domenic Denicola',
  name: 'npm-test-shrinkwrap-shared-dev-dependency',
  version: '0.0.0',
  dependencies: {
    'test-package-with-one-dep': '0.0.0'
  },
  devDependencies: {
    'test-package': '0.0.0'
  }
}

var server
test('setup', function (t) {
  setup()
  mr({ port: common.port }, function (er, s) {
    if (er) throw er
    server = s
    t.done()
  })
})

var desired = {
  name: 'npm-test-shrinkwrap-shared-dev-dependency',
  version: '0.0.0',
  dependencies: {
    'test-package-with-one-dep': {
      version: '0.0.0',
      resolved: common.registry + '/test-package-with-one-dep/-/test-package-with-one-dep-0.0.0.tgz',
      integrity: 'sha1-JWwVltusKyPRImjatagCuy42Wsg='
    },
    'test-package': {
      version: '0.0.0',
      resolved: common.registry + '/test-package/-/test-package-0.0.0.tgz',
      integrity: 'sha1-sNMrbEXCWcV4uiADdisgUTG9+9E='
    }
  }
}

test("shrinkwrap doesn't strip out the shared dependency", function (t) {
  t.plan(3)

  return common.npm(['install'], opts).spread((code) => {
    t.is(code, 0, 'install')
    return common.npm(['shrinkwrap'], opts)
  }).spread((code) => {
    t.is(code, 0, 'shrinkwrap')
    var results = JSON.parse(fs.readFileSync(`${pkg}/npm-shrinkwrap.json`))
    t.like(results.dependencies, desired.dependencies)
    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2))
}

function cleanup () {
  rimraf.sync(pkg)
}
