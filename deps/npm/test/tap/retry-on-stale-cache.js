var path = require('path')

var mr = require('npm-registry-mock')
var test = require('tap').test
var common = require('../common-tap')
var extend = Object.assign || require('util')._extend
var Tacks = require('tacks')
var Dir = Tacks.Dir
var File = Tacks.File

var workdir = path.join(__dirname, path.basename(__filename, '.js'))
var cachedir = path.join(workdir, 'cache')
var modulesdir = path.join(workdir, 'modules')
var oldModule = path.join(modulesdir, 'good-night-0.1.0.tgz')
var newModule = path.join(modulesdir, 'good-night-1.0.0.tgz')

var config = [
  '--cache', cachedir,
  '--prefix', workdir,
  '--registry', common.registry
]

var fixture = new Tacks(Dir({
  'cache': Dir(),
  'modules': Dir({
    'good-night-0.1.0.tgz': File(new Buffer(
      '1f8b0800000000000003ed934f4bc43010c57beea7187a59056dd36eff80' +
      'de85050541c1f3d8c634da4e4a925a8af8dd6db7bb8ba0e0c15559e9eff2' +
      '206f929909bc06f327143c6826f51f8d2267cf30c6d2388641c32c61ef75' +
      '4d9426e084519a25491645cbcc61e192c5d1e0ef7b90cf688d453d8cf2dd' +
      '77a65d60a707c28b0b031e61cdbd33f08452c52949515aef64729eb93652' +
      'd168323ff4d9f6bce026d7b2b11bafef11b1eb3a221a2aa6126c6da9f4e8' +
      '5e691f6e908a1a697b5ff346196995eec7023399c1c7fe95cc3999f57077' +
      'b717d7979efbeafef5a7fd2336b90f6a943484ff477a7c917f96c5bbfc87' +
      '493ae63f627138e7ff37c815195571bf52e268b1820e0d0825498055d069' +
      '6939d8521ab86f2dace0815715a0a9386f16c7e7730c676666660e9837c0' +
      'f6795d000c0000',
      'hex'
    )),
    'good-night-1.0.0.tgz': File(new Buffer(
      '1f8b0800000000000003ed954d6bc24010863dfb2bb6b9a8503793b849a0' +
      'eda5979efa052d484184252e495a331b76d78a94fef76e8cf683163cd42a' +
      '957d2e03796777268187543c7de299f0aba6d2472db1b5650020668cd81a' +
      '24117cae4bc23822ad208c93284a1208c216040318c436dff6223f31d386' +
      '2bbbca6fef69de85bcd77fc24b9b583ce4a5f04e88974939e96391e5c63b' +
      '6e9267a17421b10e030a14d6cf2742a7aaa8cc2a5b2c38e7f3f91c116d47' +
      'd3c2672697aa4eaf1425771c2725c7f579252aa90b23d5a26ed04de87f9f' +
      '3f2d52817ab9dcf0fee2f6d26bbfb6f7fdd10e8895f77ec90bb4f2ffc98c' +
      '0dfe439c7cf81fc4b5ff213070feef8254a2965341a732eb76b4cef39c12' +
      'e456eb52d82a29198dc637639f9c751fce8796eba35ea777ea0c3c14d6fe' +
      '532314f62ba9ccf6676cf21fbefcff59ed3f4b22e7ff2e60110bc37d2fe1' +
      '70381c8e9df306642df14500100000',
      'hex'
    ))
  })
}))

var server

// In this test we mock a situation where the user has a package in his cache,
// a newer version of the package is published, and the user tried to install
// said new version while requestion that the cache be used.
// npm should see that it doesn't have the package in its cache and hit the
// registry.
var onlyOldMetadata = {
  'name': 'good-night',
  'dist-tags': {
    'latest': '0.1.0'
  },
  'versions': {
    '0.1.0': {
      'name': 'good-night',
      'version': '0.1.0',
      'dist': {
        'shasum': '2a746d49dd074ba0ec2d6ff13babd40c658d89eb',
        'tarball': 'http://localhost:' + common.port + '/good-night/-/good-night-0.1.0.tgz'
      }
    }
  }
}

var oldAndNewMetadata = extend({}, onlyOldMetadata)
oldAndNewMetadata['dist-tags'] = { latest: '1.0.0' }
oldAndNewMetadata.versions = extend({
  '1.0.0': {
    'name': 'good-night',
    'version': '1.0.0',
    'dist': {
      'shasum': 'f377bf002a0a8fc4085d347a160a790b76896bc3',
      'tarball': 'http://localhost:' + common.port + '/good-night/-/good-night-1.0.0.tgz'
    }
  }
}, oldAndNewMetadata.versions)

function setup () {
  cleanup()
  fixture.create(workdir)
}

function cleanup () {
  fixture.remove(workdir)
}

test('setup', function (t) {
  setup()
  t.end()
})

test('setup initial server', function (t) {
  mr({
    port: common.port,
    throwOnUnmatched: true
  }, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s

    server.get('/good-night')
      .many({ min: 1, max: 1 })
      .reply(200, onlyOldMetadata)
    server.get('/good-night/-/good-night-0.1.0.tgz')
      .many({ min: 1, max: 1 })
      .replyWithFile(200, oldModule)

    t.end()
  })
})

test('install initial version', function (t) {
  common.npm(config.concat([
    'install', 'good-night'
  ]), {stdio: 'inherit'}, function (err, code) {
    if (err) throw err
    t.is(code, 0, 'initial install succeeded')
    server.done()
    t.end()
  })
})

test('cleanup initial server', function (t) {
  server.close()
  t.end()
})

test('setup new server', function (t) {
  mr({
    port: common.port,
    throwOnUnmatched: true
  }, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s

    server.get('/good-night')
      .many({ min: 1, max: 1 })
      .reply(200, oldAndNewMetadata)

    server.get('/good-night/-/good-night-1.0.0.tgz')
      .many({ min: 1, max: 1 })
      .replyWithFile(200, newModule)

    t.end()
  })
})

test('install new version', function (t) {
  common.npm(config.concat([
    '--prefer-offline',
    'install', 'good-night@1.0.0'
  ]), {}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.equal(stderr, '', 'no error output')
    t.is(code, 0, 'install succeeded')

    t.end()
  })
})

test('install does not hit server again', function (t) {
  // The mock server route definitions ensure we don't hit the server again
  common.npm(config.concat([
    '--prefer-offline',
    '--parseable',
    'install', 'good-night'
  ]), {stdio: [0, 'pipe', 2]}, function (err, code, stdout) {
    if (err) throw err
    t.is(code, 0, 'install succeeded')

    t.match(stdout, /^update\tgood-night\t1.0.0\t/, 'installed latest version')
    server.done()
    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})
