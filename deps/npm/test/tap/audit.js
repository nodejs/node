'use strict'

const BB = require('bluebird')

const common = BB.promisifyAll(require('../common-tap.js'))
const mr = BB.promisify(require('npm-registry-mock'))
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const Tacks = require('tacks')
const tap = require('tap')
const test = tap.test

const Dir = Tacks.Dir
const File = Tacks.File
const testDir = path.join(__dirname, path.basename(__filename, '.js'))

const EXEC_OPTS = { cwd: testDir }

tap.tearDown(function () {
  process.chdir(__dirname)
  try {
    rimraf.sync(testDir)
  } catch (e) {
    if (process.platform !== 'win32') {
      throw e
    }
  }
})

function tmock (t) {
  return mr({port: common.port}).then(s => {
    t.tearDown(function () {
      s.done()
      s.close()
      rimraf.sync(testDir)
    })
    return s
  })
}

test('exits with zero exit code for vulnerabilities below the `audit-level` flag', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      dependencies: {
        baddep: '1.0.0'
      }
    })
  }))
  fixture.create(testDir)
  return tmock(t).then(srv => {
    srv.filteringRequestBody(req => 'ok')
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, 'yeah')
    srv.get('/baddep').twice().reply(200, {
      name: 'baddep',
      'dist-tags': {
        'latest': '1.2.3'
      },
      versions: {
        '1.0.0': {
          name: 'baddep',
          version: '1.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'baddep',
          version: '1.2.3',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-1.2.3.tgz'
          }
        }
      }
    })
    return common.npm([
      'install',
      '--audit',
      '--json',
      '--package-lock-only',
      '--registry', common.registry,
      '--cache', path.join(testDir, 'npm-cache')
    ], EXEC_OPTS).then(([code, stdout, stderr]) => {
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'update',
          module: 'baddep',
          target: '1.2.3',
          resolves: [{path: 'baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            low: 1
          }
        }
      })
      return common.npm([
        'audit',
        '--audit-level', 'high',
        '--json',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
      })
    })
  })
})

test('exits with non-zero exit code for vulnerabilities at the `audit-level` flag', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      dependencies: {
        baddep: '1.0.0'
      }
    })
  }))
  fixture.create(testDir)
  return tmock(t).then(srv => {
    srv.filteringRequestBody(req => 'ok')
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, 'yeah')
    srv.get('/baddep').twice().reply(200, {
      name: 'baddep',
      'dist-tags': {
        'latest': '1.2.3'
      },
      versions: {
        '1.0.0': {
          name: 'baddep',
          version: '1.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'baddep',
          version: '1.2.3',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-1.2.3.tgz'
          }
        }
      }
    })
    return common.npm([
      'install',
      '--audit',
      '--json',
      '--package-lock-only',
      '--registry', common.registry,
      '--cache', path.join(testDir, 'npm-cache')
    ], EXEC_OPTS).then(([code, stdout, stderr]) => {
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'update',
          module: 'baddep',
          target: '1.2.3',
          resolves: [{path: 'baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            high: 1
          }
        }
      })
      return common.npm([
        'audit',
        '--audit-level', 'high',
        '--json',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 1, 'exited OK')
      })
    })
  })
})

test('exits with non-zero exit code for vulnerabilities at the `audit-level` flag', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      dependencies: {
        baddep: '1.0.0'
      }
    })
  }))
  fixture.create(testDir)
  return tmock(t).then(srv => {
    srv.filteringRequestBody(req => 'ok')
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, 'yeah')
    srv.get('/baddep').twice().reply(200, {
      name: 'baddep',
      'dist-tags': {
        'latest': '1.2.3'
      },
      versions: {
        '1.0.0': {
          name: 'baddep',
          version: '1.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'baddep',
          version: '1.2.3',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-1.2.3.tgz'
          }
        }
      }
    })
    return common.npm([
      'install',
      '--audit',
      '--json',
      '--package-lock-only',
      '--registry', common.registry,
      '--cache', path.join(testDir, 'npm-cache')
    ], EXEC_OPTS).then(([code, stdout, stderr]) => {
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'update',
          module: 'baddep',
          target: '1.2.3',
          resolves: [{path: 'baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            high: 1
          }
        }
      })
      return common.npm([
        'audit',
        '--audit-level', 'moderate',
        '--json',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 1, 'exited OK')
      })
    })
  })
})

test('cleanup', t => {
  return rimraf(testDir)
})
