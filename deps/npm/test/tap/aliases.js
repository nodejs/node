'use strict'

const BB = require('bluebird')

const common = require('../common-tap.js')
const fs = require('graceful-fs')
const mockTar = require('../util/mock-tarball.js')
const mr = common.fakeRegistry.compat
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const Tacks = require('tacks')
const { test } = require('tap')

const { Dir, File } = Tacks
const readdirAsync = BB.promisify(fs.readdir)
const readFileAsync = BB.promisify(fs.readFile)

const testDir = path.join(__dirname, path.basename(__filename, '.js'))

let server
test('setup', t => {
  mr({}, (err, s) => {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('installs an npm: protocol alias package', t => {
  const fixture = new Tacks(Dir({
    'package.json': File({})
  }))
  fixture.create(testDir)
  const packument = {
    name: 'foo',
    'dist-tags': { latest: '1.2.4' },
    versions: {
      '1.2.3': {
        name: 'foo',
        version: '1.2.3',
        dist: {
          tarball: `${server.registry}/foo/-/foo-1.2.3.tgz`
        }
      },
      '1.2.4': {
        name: 'foo',
        version: '1.2.4',
        dist: {
          tarball: `${server.registry}/foo/-/foo-1.2.4.tgz`
        }
      }
    }
  }
  server.get('/foo').reply(200, packument)
  return mockTar({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.2.3'
    })
  }).then(tarball => {
    server.get('/foo/-/foo-1.2.3.tgz').reply(200, tarball)
    server.get('/foo/-/foo-1.2.4.tgz').reply(200, tarball)
    return common.npm([
      'install', 'foo@1.2.3',
      '--cache', path.join(testDir, 'npmcache'),
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.equal(code, 0)
    t.comment(stdout)
    t.comment(stderr)
    return common.npm([
      'install', 'bar@npm:foo@1.2.3',
      '--cache', path.join(testDir, 'npmcache'),
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.equal(code, 0)
    t.comment(stdout)
    t.comment(stderr)
    t.match(stdout, /\+ foo@1\.2\.3 \(as bar\)/, 'useful message')
    return readFileAsync(
      path.join(testDir, 'node_modules', 'bar', 'package.json'),
      'utf8'
    )
  }).then(JSON.parse).then(pkg => {
    t.similar(pkg, {
      name: 'foo',
      version: '1.2.3'
    }, 'successfully installed foo as bar in node_modules')
    return common.npm(['ls', '--json'], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.comment(stdout)
    t.comment(stderr)
    t.equal(code, 0, 'ls is clean')
    t.deepEqual(JSON.parse(stdout), {
      dependencies: {
        bar: {
          version: '1.2.3',
          from: 'bar@npm:foo@1.2.3',
          resolved: 'http://localhost:1337/foo/-/foo-1.2.3.tgz'
        },
        foo: {
          version: '1.2.3',
          from: 'foo@1.2.3',
          resolved: 'http://localhost:1337/foo/-/foo-1.2.3.tgz'
        }
      }
    }, 'both dependencies listed correctly')
    return common.npm([
      'outdated', '--json',
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.equal(code, 1, 'non-zero because some packages outdated')
    t.comment(stdout)
    t.comment(stderr)
    const parsed = JSON.parse(stdout)
    t.deepEqual(parsed, {
      foo: {
        current: '1.2.3',
        wanted: '1.2.4',
        latest: '1.2.4',
        location: 'node_modules/foo'
      },
      bar: {
        current: 'npm:foo@1.2.3',
        wanted: 'npm:foo@1.2.4',
        latest: 'npm:foo@1.2.4',
        location: 'node_modules/bar'
      }
    }, 'both regular and aliased dependency reported')
    return common.npm([
      'update',
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.equal(code, 0, 'update succeeded')
    t.comment(stdout)
    t.comment(stderr)
    return common.npm(['ls', '--json'], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.equal(code, 0, 'ls succeeded')
    t.comment(stdout)
    t.comment(stderr)
    const parsed = JSON.parse(stdout)
    t.deepEqual(parsed, {
      dependencies: {
        bar: {
          version: '1.2.4',
          from: 'bar@npm:foo@1.2.4',
          resolved: 'http://localhost:1337/foo/-/foo-1.2.4.tgz'
        },
        foo: {
          version: '1.2.4',
          from: 'foo@1.2.4',
          resolved: 'http://localhost:1337/foo/-/foo-1.2.4.tgz'
        }
      }
    }, 'ls shows updated packages')
    return common.npm([
      'rm', 'bar',
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.equal(code, 0, 'rm succeeded')
    t.comment(stdout)
    t.comment(stderr)
    t.match(stdout, 'removed 1 package', 'notified of removed package')
    return readdirAsync(path.join(testDir, 'node_modules'))
  }).then(dir => {
    t.deepEqual(dir, ['foo'], 'regular foo left in place')
  }).then(() => rimraf(testDir))
})

test('installs a tarball dep as a different name than package.json', t => {
  return mockTar({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.2.3'
    })
  }).then(tarball => {
    const fixture = new Tacks(Dir({
      'package.json': File({}),
      'foo.tgz': File(tarball)
    }))
    fixture.create(testDir)
    return common.npm([
      'install', 'file:foo.tgz',
      '--cache', path.join(testDir, 'npmcache'),
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    return common.npm([
      'install', 'bar@file:foo.tgz',
      '--cache', path.join(testDir, 'npmcache'),
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.comment(stdout)
    t.comment(stderr)
    t.match(stdout, /^\+ foo@1\.2\.3 \(as bar\)/, 'useful message')
    return readFileAsync(
      path.join(testDir, 'node_modules', 'bar', 'package.json'),
      'utf8'
    )
  }).then(JSON.parse).then(pkg => {
    t.similar(pkg, {
      name: 'foo',
      version: '1.2.3'
    }, 'successfully installed foo as bar in node_modules')
    return common.npm(['ls', '--json'], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.comment(stdout)
    t.comment(stderr)
    t.similar(JSON.parse(stdout), {
      dependencies: {
        bar: {
          version: '1.2.3',
          from: 'file:foo.tgz'
        },
        foo: {
          version: '1.2.3',
          from: 'file:foo.tgz'
        }
      }
    }, 'both dependencies present')
  }).then(() => rimraf(testDir))
})

test('installs a symlink dep as a different name than package.json', t => {
  const fixture = new Tacks(Dir({
    'package.json': File({}),
    'foo': Dir({
      'package.json': File({
        name: 'foo',
        version: '1.2.3'
      })
    })
  }))
  fixture.create(testDir)
  return common.npm([
    'install', 'bar@file:foo',
    '--cache', path.join(testDir, 'npmcache'),
    '--registry', server.registry
  ], { cwd: testDir }).then(([code, stdout, stderr]) => {
    t.comment(stdout)
    t.comment(stderr)
    t.match(stdout, /^\+ foo@1\.2\.3 \(as bar\)/, 'useful message')
    return readFileAsync(
      path.join(testDir, 'node_modules', 'bar', 'package.json'),
      'utf8'
    )
  }).then(JSON.parse).then(pkg => {
    t.similar(pkg, {
      name: 'foo',
      version: '1.2.3'
    }, 'successfully installed foo as bar in node_modules')
  }).then(() => rimraf(testDir))
})

test('cleanup', t => {
  server.close()
  return rimraf(testDir)
})

test('npm audit supports aliases')
test('npm audit fix supports aliases')
