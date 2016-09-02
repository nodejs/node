var path = require('path')
var test = require('tap').test
var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var requireInject = require('require-inject')

var pkg = path.join(__dirname, '/local-dir')
var cache = path.join(pkg, '/cache')
var tmp = path.join(pkg, '/tmp')
var prefix = path.join(pkg, '/prefix')

var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

test('addLocal directory race on Windows', function (t) {
  setup()
  var p = {
    name: 'test',
    version: '1.0.0',
    type: 'directory',
    spec: pkg
  }
  var fixture = new Tacks(
    Dir({
      'package.json': File(p)
    })
  )
  var addLocal = requireInject('../../lib/cache/add-local', {
    '../../lib/npm.js': {
      cache: cache,
      tmp: tmp,
      prefix: prefix
    },
    '../../lib/cache/get-stat': function (cb) {
      cb(null, {})
    },
    chownr: function (x, y, z, cb) {
      cb(new Error('chownr should never have been called'))
    },
    '../../lib/cache/add-local-tarball.js': function (tgz, data, shasum, cb) {
      cb(null)
    },
    '../../lib/utils/lifecycle.js': function (data, cycle, p, cb) {
      cb(null)
    },
    '../../lib/utils/tar.js': {
      pack: function (tgz, p, data, cb) {
        cb(null)
      }
    },
    'sha': {
      get: function (tgz, cb) {
        cb(null, 'deadbeef')
      }
    }
  })

  fixture.create(pkg)
  addLocal(p, null, function (err) {
    t.ifErr(err, 'addLocal completed without error')
    t.done()
  })
})

test('addLocal temporary cache file race', function (t) {
  // See https://github.com/npm/npm/issues/12669
  setup()
  var p = {
    name: 'test',
    version: '1.0.0',
    type: 'directory',
    spec: pkg
  }
  var fixture = new Tacks(
    Dir({
      'package.json': File(p)
    })
  )
  var addLocal = requireInject('../../lib/cache/add-local', {
    // basic setup/mock stuff
    '../../lib/npm.js': {
      cache: cache,
      tmp: tmp,
      prefix: prefix
    },
    '../../lib/cache/add-local-tarball.js': function (tgz, data, shasum, cb) {
      cb(null)
    },
    '../../lib/utils/lifecycle.js': function (data, cycle, p, cb) {
      cb(null)
    },
    '../../lib/utils/tar.js': {
      pack: function (tgz, p, data, cb) {
        cb(null)
      }
    },
    'sha': {
      get: function (tgz, cb) {
        cb(null, 'deadbeef')
      }
    },

    // Test-specific mocked values to simulate race.
    '../../lib/cache/get-stat': function (cb) {
      cb(null, {uid: 1, gid: 2})
    },
    chownr: function (x, y, z, cb) {
      // Simulate a race condition between `tar.pack` and `chownr`
      // where the latter will return `ENOENT` when an async process
      // removes a file that its internal `fs.readdir` listed.
      cb({code: 'ENOENT'})
    }
  })

  fixture.create(pkg)
  addLocal(p, null, function (err) {
    t.ifErr(err, 'addLocal completed without error')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

function setup () {
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
