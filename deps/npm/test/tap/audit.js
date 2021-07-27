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
const testDir = common.pkg

const EXEC_OPTS = { cwd: testDir }

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

const quickAuditResult = {
  actions: [],
  advisories: {
    '1316': {
      findings: [
        {
          version: '1.0.0',
          paths: [
            'baddep'
          ]
        }
      ],
      'id': 1316,
      'created': '2019-11-14T15:29:41.991Z',
      'updated': '2019-11-14T19:35:30.677Z',
      'deleted': null,
      'title': 'Arbitrary Code Execution',
      'found_by': {
        'link': '',
        'name': 'François Lajeunesse-Robert',
        'email': ''
      },
      'reported_by': {
        'link': '',
        'name': 'François Lajeunesse-Robert',
        'email': ''
      },
      'module_name': 'baddep',
      'cves': [],
      'vulnerable_versions': '<4.5.2',
      'patched_versions': '>=4.5.2',
      'overview': 'a nice overview of the advisory',
      'recommendation': 'how you should fix it',
      'references': '',
      'access': 'public',
      'severity': 'high',
      'cwe': 'CWE-79',
      'metadata': {
        'module_type': '',
        'exploitability': 6,
        'affected_components': ''
      },
      'url': 'https://npmjs.com/advisories/1234542069'
    }
  },
  'muted': [],
  'metadata': {
    'vulnerabilities': {
      'info': 0,
      'low': 0,
      'moderate': 0,
      'high': 1,
      'critical': 0
    },
    'dependencies': 1,
    'devDependencies': 0,
    'totalDependencies': 1
  }
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
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, quickAuditResult)
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
      const result = JSON.parse(stdout)
      t.same(result.audit, quickAuditResult, 'printed quick audit result')
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

test('shows quick audit results summary for human', t => {
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
    srv.post('/-/npm/v1/security/audits/quick', 'ok').reply(200, quickAuditResult)
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
      '--no-json',
      '--package-lock-only',
      '--registry', common.registry,
      '--cache', path.join(testDir, 'npm-cache')
    ], EXEC_OPTS).then(([code, stdout, stderr]) => {
      t.match(stdout, new RegExp('added 1 package and audited 1 package in .*\\n' +
        'found 1 high severity vulnerability\\n' +
        '  run `npm audit fix` to fix them, or `npm audit` for details\\n'),
      'shows quick audit result')
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

test('exits with zero exit code for vulnerabilities in devDependencies when running with production flag', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      dependencies: {
        gooddep: '1.0.0'
      },
      devDependencies: {
        baddep: '1.0.0'
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
      '--production',
      '--package-lock-only',
      '--registry', common.registry,
      '--cache', path.join(testDir, 'npm-cache')
    ], EXEC_OPTS).then(([code, stdout, stderr]) => {
      srv.filteringRequestBody(req => 'ok')
      srv.post('/-/npm/v1/security/audits', 'ok').reply(200, {
        actions: [],
        metadata: {
          vulnerabilities: {}
        }
      })
      return common.npm([
        'audit',
        '--json',
        '--production',
        '--registry', common.registry,
        '--cache', path.join(testDir, 'npm-cache')
      ], EXEC_OPTS).then(([code, stdout, stderr]) => {
        t.equal(code, 0, 'exited OK')
      })
    })
  })
})

test('exits with non-zero exit code for vulnerabilities in dependencies when running with production flag', t => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'foo',
      version: '1.0.0',
      dependencies: {
        baddep: '1.0.0'
      },
      devDependencies: {
        gooddep: '1.0.0'
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
      '--production',
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
        '--json',
        '--production',
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
