var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-test-cli-with-broken-package-lock',
  description: 'fixture',
  version: '0.0.0',
  dependencies: {
    optimist: '0.6.0'
  }
}

var brokenLockfile = {
  name: 'install-test-cli-with-broken-package-lock',
  version: '0.0.0',
  lockfileVersion: 1,
  requires: true,
  dependencies: {
    optimist: {
      version: '0.6.0',
      resolved: 'https://registry.npmjs.org/optimist/-/optimist-0.6.0.tgz',
      integrity: 'sha1-aUJIJvNAX3nxQub8PZrljU27kgA=',
      requires: {
        minimist: '~0.0.1',
        wordwrap: '~0.0.2'
      }
    },
    wordwrap: {
      version: '0.0.3',
      resolved: 'https://registry.npmjs.org/wordwrap/-/wordwrap-0.0.3.tgz',
      integrity: 'sha1-o9XabNXAvAAI03I0u68b7WMFkQc='
    }
  }
}

var expected = {
  name: 'install-test-cli-with-broken-package-lock',
  version: '0.0.0',
  lockfileVersion: 1,
  requires: true,
  dependencies: {
    minimist: {
      version: '0.0.10',
      resolved: 'https://registry.npmjs.org/minimist/-/minimist-0.0.10.tgz',
      integrity: 'sha1-3j+YVD2/lggr5IrRoMfNqDYwHc8='
    },
    optimist: {
      version: '0.6.0',
      resolved: 'https://registry.npmjs.org/optimist/-/optimist-0.6.0.tgz',
      integrity: 'sha1-aUJIJvNAX3nxQub8PZrljU27kgA=',
      requires: {
        minimist: '~0.0.1',
        wordwrap: '~0.0.2'
      }
    },
    wordwrap: {
      version: '0.0.3',
      resolved: 'https://registry.npmjs.org/wordwrap/-/wordwrap-0.0.3.tgz',
      integrity: 'sha1-o9XabNXAvAAI03I0u68b7WMFkQc='
    }
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

test('\'npm install-test\' should repair package-lock.json', function (t) {
  common.npm(['install-test'], EXEC_OPTS, function (err, code, stderr, stdout) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 0, 'npm install did not raise error code')
    var lockfile = JSON.parse(fs.readFileSync(path.join(pkg, 'package-lock.json')))
    t.same(
      lockfile,
      expected,
      'package-lock.json should be repaired'
    )
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  fs.writeFileSync(
    path.join(pkg, 'package-lock.json'),
    JSON.stringify(brokenLockfile, null, 2)
  )
  process.chdir(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
