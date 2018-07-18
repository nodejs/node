'use strict'

const BB = require('bluebird')

const common = BB.promisifyAll(require('../common-tap.js'))
const fs = require('fs')
const mr = common.fakeRegistry.compat
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

test('fixes shallow vulnerabilities', t => {
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
      t.equal(code, 0, 'exited OK')
      t.comment(stderr)
      t.similar(JSON.parse(stdout), {
        added: [{
          action: 'add',
          name: 'baddep',
          version: '1.0.0'
        }]
      }, 'installed bad version')
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
            critical: 1
          }
        }
      })
      return common.npm([
        'audit', 'fix',
        '--package-lock-only',
        '--json',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
        t.comment(stderr)
        t.similar(JSON.parse(stdout), {
          added: [{
            action: 'add',
            name: 'baddep',
            version: '1.2.3'
          }]
        }, 'reported dependency update')
        t.similar(JSON.parse(fs.readFileSync(path.join(testDir, 'package-lock.json'), 'utf8')), {
          dependencies: {
            baddep: {
              version: '1.2.3',
              resolved: common.registry + '/idk/-/idk-1.2.3.tgz',
              integrity: 'sha1-3q2+7w=='
            }
          }
        }, 'pkglock updated correctly')
      })
    })
  })
})

test('fixes nested dep vulnerabilities', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      dependencies: {
        gooddep: '^1.0.0'
      }
    })
  }))
  fixture.create(testDir)
  return tmock(t).then(srv => {
    srv.filteringRequestBody(req => 'ok')
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, 'yeah')
    srv.get('/baddep').reply(200, {
      name: 'baddep',
      'dist-tags': {
        'latest': '1.0.0'
      },
      versions: {
        '1.0.0': {
          name: 'baddep',
          version: '1.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'c0ffee',
            integrity: 'sha1-c0ffee',
            tarball: common.registry + '/baddep/-/baddep-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'baddep',
          version: '1.2.3',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'bada55',
            integrity: 'sha1-bada55',
            tarball: common.registry + '/baddep/-/baddep-1.2.3.tgz'
          }
        }
      }
    })

    srv.get('/gooddep').reply(200, {
      name: 'gooddep',
      'dist-tags': {
        'latest': '1.0.0'
      },
      versions: {
        '1.0.0': {
          name: 'gooddep',
          version: '1.0.0',
          dependencies: {
            baddep: '^1.0.0'
          },
          _hasShrinkwrap: false,
          dist: {
            shasum: '1234',
            tarball: common.registry + '/gooddep/-/gooddep-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'gooddep',
          version: '1.2.3',
          _hasShrinkwrap: false,
          dependencies: {
            baddep: '^1.0.0'
          },
          dist: {
            shasum: '123456',
            tarball: common.registry + '/gooddep/-/gooddep-1.2.3.tgz'
          }
        }
      }
    })

    return common.npm([
      'install',
      '--audit',
      '--json',
      '--global-style',
      '--package-lock-only',
      '--registry', common.registry,
      '--cache', path.join(testDir, 'npm-cache')
    ], EXEC_OPTS).then(([code, stdout, stderr]) => {
      t.equal(code, 0, 'exited OK')
      t.comment(stderr)
      t.similar(JSON.parse(stdout), {
        added: [{
          action: 'add',
          name: 'baddep',
          version: '1.0.0'
        }, {
          action: 'add',
          name: 'gooddep',
          version: '1.0.0'
        }]
      }, 'installed bad version')
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'update',
          module: 'baddep',
          target: '1.2.3',
          resolves: [{path: 'gooddep>baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            critical: 1
          }
        }
      })
      return common.npm([
        'audit', 'fix',
        '--package-lock-only',
        '--offline',
        '--json',
        '--global-style',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
        t.comment(stderr)
        t.similar(JSON.parse(stdout), {
          added: [{
            action: 'add',
            name: 'baddep',
            version: '1.2.3'
          }, {
            action: 'add',
            name: 'gooddep',
            version: '1.0.0'
          }]
        }, 'reported dependency update')
        t.similar(JSON.parse(fs.readFileSync(path.join(testDir, 'package-lock.json'), 'utf8')), {
          dependencies: {
            gooddep: {
              version: '1.0.0',
              resolved: common.registry + '/gooddep/-/gooddep-1.0.0.tgz',
              integrity: 'sha1-EjQ=',
              requires: {
                baddep: '^1.0.0'
              },
              dependencies: {
                baddep: {
                  version: '1.2.3',
                  resolved: common.registry + '/baddep/-/baddep-1.2.3.tgz',
                  integrity: 'sha1-bada55'
                }
              }
            }
          }
        }, 'pkglock updated correctly')
      })
    })
  })
})

test('no semver-major without --force', t => {
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
        'latest': '2.0.0'
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
        '2.0.0': {
          name: 'baddep',
          version: '2.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-2.0.0.tgz'
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
      t.equal(code, 0, 'exited OK')
      t.comment(stderr)
      t.similar(JSON.parse(stdout), {
        added: [{
          action: 'add',
          name: 'baddep',
          version: '1.0.0'
        }]
      }, 'installed bad version')
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'install',
          module: 'baddep',
          target: '2.0.0',
          isMajor: true,
          resolves: [{path: 'baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            critical: 1
          }
        }
      })
      return common.npm([
        'audit', 'fix',
        '--package-lock-only',
        '--registry', common.registry,
        '--loglevel=warn',
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
        t.comment(stderr)
        t.match(stdout, /breaking changes/, 'informs about semver-major')
        t.match(stdout, /npm audit fix --force/, 'recommends --force')
        t.similar(JSON.parse(fs.readFileSync(path.join(testDir, 'package-lock.json'), 'utf8')), {
          dependencies: {
            baddep: {
              version: '1.0.0'
            }
          }
        }, 'pkglock not updated')
      })
    })
  })
})

test('semver-major when --force', t => {
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
        'latest': '2.0.0'
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
        '2.0.0': {
          name: 'baddep',
          version: '2.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-2.0.0.tgz'
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
      t.equal(code, 0, 'exited OK')
      t.comment(stderr)
      t.similar(JSON.parse(stdout), {
        added: [{
          action: 'add',
          name: 'baddep',
          version: '1.0.0'
        }]
      }, 'installed bad version')
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'install',
          module: 'baddep',
          target: '2.0.0',
          isMajor: true,
          resolves: [{path: 'baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            critical: 1
          }
        }
      })
      return common.npm([
        'audit', 'fix',
        '--package-lock-only',
        '--registry', common.registry,
        '--force',
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
        t.comment(stderr)
        t.match(stdout, /breaking changes/, 'informs about semver-major')
        t.similar(JSON.parse(fs.readFileSync(path.join(testDir, 'package-lock.json'), 'utf8')), {
          dependencies: {
            baddep: {
              version: '2.0.0'
            }
          }
        }, 'pkglock not updated')
      })
    })
  })
})

test('no installs for review-requires', t => {
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
    srv.filteringRequestBody(req => 'k')
    srv.post('/-/npm/v1/security/audits/quick', 'k').reply(200, 'yeah')
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
      t.equal(code, 0, 'exited OK')
      t.comment(stderr)
      t.similar(JSON.parse(stdout), {
        added: [{
          action: 'add',
          name: 'baddep',
          version: '1.0.0'
        }]
      }, 'installed bad version')
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'review',
          module: 'baddep',
          target: '1.2.3',
          resolves: [{path: 'baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            critical: 1
          }
        }
      })
      return common.npm([
        'audit', 'fix',
        '--package-lock-only',
        '--json',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
        t.comment(stderr)
        t.similar(JSON.parse(stdout), {
          added: [{
            action: 'add',
            name: 'baddep',
            version: '1.0.0'
          }]
        }, 'no update for dependency')
      })
    })
  })
})

test('nothing to fix', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      dependencies: {
        gooddep: '1.0.0'
      }
    })
  }))
  fixture.create(testDir)
  return tmock(t).then(srv => {
    srv.filteringRequestBody(req => 'ok')
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, 'yeah')
    srv.get('/gooddep').twice().reply(200, {
      name: 'gooddep',
      'dist-tags': {
        'latest': '1.2.3'
      },
      versions: {
        '1.0.0': {
          name: 'gooddep',
          version: '1.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'deadbeef',
            tarball: common.registry + '/idk/-/idk-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'gooddep',
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
      t.equal(code, 0, 'exited OK')
      t.comment(stderr)
      t.similar(JSON.parse(stdout), {
        added: [{
          action: 'add',
          name: 'gooddep',
          version: '1.0.0'
        }]
      }, 'installed good version')
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [],
        metadata: {
          vulnerabilities: { }
        }
      })
      return common.npm([
        'audit', 'fix',
        '--package-lock-only',
        '--json',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
        t.comment(stderr)
        t.similar(JSON.parse(stdout), {
          added: [{
            action: 'add',
            name: 'gooddep',
            version: '1.0.0'
          }]
        }, 'nothing to update')
      })
    })
  })
})

test('preserves deep deps dev: true', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      devDependencies: {
        gooddep: '^1.0.0'
      }
    })
  }))
  fixture.create(testDir)
  return tmock(t).then(srv => {
    srv.filteringRequestBody(req => 'ok')
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, 'yeah')
    srv.get('/baddep').reply(200, {
      name: 'baddep',
      'dist-tags': {
        'latest': '1.0.0'
      },
      versions: {
        '1.0.0': {
          name: 'baddep',
          version: '1.0.0',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'c0ffee',
            integrity: 'sha1-c0ffee',
            tarball: common.registry + '/baddep/-/baddep-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'baddep',
          version: '1.2.3',
          _hasShrinkwrap: false,
          dist: {
            shasum: 'bada55',
            integrity: 'sha1-bada55',
            tarball: common.registry + '/baddep/-/baddep-1.2.3.tgz'
          }
        }
      }
    })

    srv.get('/gooddep').reply(200, {
      name: 'gooddep',
      'dist-tags': {
        'latest': '1.0.0'
      },
      versions: {
        '1.0.0': {
          name: 'gooddep',
          version: '1.0.0',
          dependencies: {
            baddep: '^1.0.0'
          },
          _hasShrinkwrap: false,
          dist: {
            shasum: '1234',
            tarball: common.registry + '/gooddep/-/gooddep-1.0.0.tgz'
          }
        },
        '1.2.3': {
          name: 'gooddep',
          version: '1.2.3',
          _hasShrinkwrap: false,
          dependencies: {
            baddep: '^1.0.0'
          },
          dist: {
            shasum: '123456',
            tarball: common.registry + '/gooddep/-/gooddep-1.2.3.tgz'
          }
        }
      }
    })

    return common.npm([
      'install',
      '--audit',
      '--json',
      '--global-style',
      '--package-lock-only',
      '--registry', common.registry,
      '--cache', path.join(testDir, 'npm-cache')
    ], EXEC_OPTS).then(([code, stdout, stderr]) => {
      t.equal(code, 0, 'exited OK')
      t.comment(stderr)
      t.similar(JSON.parse(stdout), {
        added: [{
          action: 'add',
          name: 'baddep',
          version: '1.0.0'
        }, {
          action: 'add',
          name: 'gooddep',
          version: '1.0.0'
        }]
      }, 'installed bad version')
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [{
          action: 'update',
          module: 'baddep',
          target: '1.2.3',
          resolves: [{path: 'gooddep>baddep'}]
        }],
        metadata: {
          vulnerabilities: {
            critical: 1
          }
        }
      })
      return common.npm([
        'audit', 'fix',
        '--package-lock-only',
        '--offline',
        '--json',
        '--global-style',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
        t.comment(stderr)
        t.similar(JSON.parse(stdout), {
          added: [{
            action: 'add',
            name: 'baddep',
            version: '1.2.3'
          }, {
            action: 'add',
            name: 'gooddep',
            version: '1.0.0'
          }]
        }, 'reported dependency update')
        t.similar(JSON.parse(fs.readFileSync(path.join(testDir, 'package-lock.json'), 'utf8')), {
          dependencies: {
            gooddep: {
              dev: true,
              version: '1.0.0',
              resolved: common.registry + '/gooddep/-/gooddep-1.0.0.tgz',
              integrity: 'sha1-EjQ=',
              requires: {
                baddep: '^1.0.0'
              },
              dependencies: {
                baddep: {
                  dev: true,
                  version: '1.2.3',
                  resolved: common.registry + '/baddep/-/baddep-1.2.3.tgz',
                  integrity: 'sha1-bada55'
                }
              }
            }
          }
        }, 'pkglock updated correctly')
      })
    })
  })
})

test('cleanup', t => {
  return rimraf(testDir)
})
