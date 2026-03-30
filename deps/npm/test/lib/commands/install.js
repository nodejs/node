const fs = require('node:fs')
const tspawk = require('../../fixtures/tspawk')
const {
  cleanCwd,
  cleanTime,
  cleanDate,
  cleanPackumentCache,
} = require('../../fixtures/clean-snapshot.js')

const path = require('node:path')
const t = require('tap')

t.cleanSnapshot = (str) => cleanPackumentCache(cleanDate(cleanTime(cleanCwd(str))))

const {
  loadNpmWithRegistry: loadMockNpm,
  workspaceMock,
} = require('../../fixtures/mock-npm')

// tspawk calls preventUnmatched which assures that no scripts run if we don't mock any
const spawk = tspawk(t)

const abbrev = {
  'package.json': '{"name": "abbrev", "version": "1.0.0"}',
  test: 'test file',
}

const packageJson = {
  name: '@npmcli/test-package',
  version: '1.0.0',
  dependencies: {
    abbrev: '^1.0.0',
  },
}

t.test('exec commands', async t => {
  await t.test('with args does not run lifecycle scripts', async t => {
    const { npm, registry } = await loadMockNpm(t, {
      config: {
        audit: false,
      },
      prefixDir: {
        'package.json': JSON.stringify({
          ...packageJson,
          scripts: {
            install: 'echo install',
          },
        }),
        abbrev,
      },
    })
    const manifest = registry.manifest({ name: 'abbrev' })
    await registry.package({ manifest })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.prefix, 'abbrev'),
    })

    await npm.exec('install', ['abbrev'])
  })

  await t.test('without args runs lifecycle scripts', async t => {
    const lifecycleScripts = [
      'preinstall',
      'install',
      'postinstall',
      'prepublish',
      'preprepare',
      'prepare',
      'postprepare',
    ]
    const scripts = {}
    for (const script of lifecycleScripts) {
      spawk.spawn(/.*/, a => {
        runOrder.push(script)
        return a.includes(`${script} lifecycle script`)
      })
      scripts[script] = `${script} lifecycle script`
    }
    const { npm, registry } = await loadMockNpm(t, {
      config: {
        audit: false,
      },
      prefixDir: {
        'package.json': JSON.stringify({
          ...packageJson,
          scripts,
        }),
        abbrev,
      },
    })
    const runOrder = []
    const manifest = registry.manifest({ name: 'abbrev' })
    await registry.package({ manifest })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.prefix, 'abbrev'),
    })

    await npm.exec('install')
    t.strictSame(lifecycleScripts, runOrder, 'all script ran in the correct order')
  })

  await t.test('should ignore scripts with --ignore-scripts', async t => {
    const { npm, registry } = await loadMockNpm(t, {
      config: {
        'ignore-scripts': true,
        audit: false,
      },
      prefixDir: {
        'package.json': JSON.stringify({
          ...packageJson,
          scripts: {
            install: 'echo install',
          },
        }),
        abbrev,
      },
    })
    const manifest = registry.manifest({ name: 'abbrev' })
    await registry.package({ manifest })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.prefix, 'abbrev'),
    })

    await npm.exec('install')
  })

  await t.test('should not self-install package if prefix is the same as CWD', async t => {
    let REIFY_CALLED_WITH = null
    const { npm } = await loadMockNpm(t, {
      mocks: {
        '{LIB}/utils/reify-finish.js': async () => {},
        '@npmcli/run-script': () => {},
        '@npmcli/arborist': function () {
          this.reify = (opts) => {
            REIFY_CALLED_WITH = opts
          }
        },
      },
      prefixOverride: process.cwd(),
    })

    await npm.exec('install')
    t.equal(REIFY_CALLED_WITH.add.length, 0, 'did not install current directory as a dependency')
  })

  await t.test('should not install invalid global package name', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        global: true,
      },
    })
    await t.rejects(
      npm.exec('install', ['']),
      /Usage:/,
      'should not install invalid package name'
    )
  })

  await t.test('npm i -g npm engines check success', async t => {
    const { npm, registry } = await loadMockNpm(t, {
      prefixDir: {
        npm: {
          'package.json': JSON.stringify({ name: 'npm', version: '1.0.0' }),
          'index.js': 'console.log("this is npm")',
        },
      },
      config: { global: true },
    })
    const manifest = registry.manifest({
      name: 'npm',
      packuments: [{ version: '1.0.0', engines: { node: '>1' } }],
    })
    await registry.package({ manifest, times: 2 })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.localPrefix, 'npm'),
    })
    await npm.exec('install', ['npm'])
    t.ok('No exceptions happen')
  })

  await t.test('npm i -g npm engines check failure', async t => {
    const { npm, registry } = await loadMockNpm(t, {
      prefixDir: {
        npm: {
          'package.json': JSON.stringify({ name: 'npm', version: '1.0.0' }),
          'index.js': 'console.log("this is the npm we are installing")',
        },
      },
      config: { global: true },
    })

    const manifest = registry.manifest({
      name: 'npm',
      packuments: [{ version: '1.0.0', engines: { node: '~1' } }],
    })
    await registry.package({ manifest })
    await t.rejects(
      npm.exec('install', ['npm']),
      {
        message: 'Unsupported engine',
        pkgid: 'npm@1.0.0',
        current: {
          node: process.version,
          npm: '1.0.0',
        },
        required: {
          node: '~1',
        },
        code: 'EBADENGINE',
      }
    )
  })

  await t.test('npm i -g npm engines check failure forced override', async t => {
    const { npm, registry } = await loadMockNpm(t, {
      prefixDir: {
        npm: {
          'package.json': JSON.stringify({ name: 'npm', version: '1.0.0' }),
          'index.js': 'console.log("this is npm")',
        },
      },
      config: { global: true, force: true },
    })
    const manifest = registry.manifest({
      name: 'npm',
      packuments: [{ version: '1.0.0', engines: { node: '~1' } }],
    })
    await registry.package({ manifest, times: 2 })
    await registry.tarball({
      manifest: manifest.versions['1.0.0'],
      tarball: path.join(npm.localPrefix, 'npm'),
    })
    await npm.exec('install', ['npm'])
    t.ok('No exceptions happen')
  })

  t.test('allow-git=none', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        'allow-git': 'none',
      },
    })
    await t.rejects(
      npm.exec('install', ['npm/npm']),
      {
        code: 'EALLOWGIT',
        message: 'Fetching packages of type "git" have been disabled',
        package: 'github:npm/npm',
      }
    )
  })

  t.test('allow-git=root refuses non-root git dependency', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        'allow-git': 'none',
      },
      prefixDir: {
        'package.json': JSON.stringify({ name: '@npmcli/test-package', version: '1.0.0' }),
        abbrev: {
          'package.json': JSON.stringify({ name: 'abbrev', version: '1.0.0', dependencies: { npm: 'npm/npm' } }),
        },
      },
    })
    await t.rejects(
      npm.exec('install', ['./abbrev']),
      /Fetching packages of type "git" have been disabled/
    )
  })
})

t.test('completion', async t => {
  const mockComp = async (t, { noChdir } = {}) => loadMockNpm(t, {
    command: 'install',
    prefixDir: {
      arborist: {
        'package.json': '{}',
      },
      'arborist.txt': 'just a file',
      'other-dir': { a: 'a' },
    },
    ...(noChdir ? { chdir: false } : {}),
  })

  await t.test('completion to folder - has a match', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './ar' })
    t.strictSame(res, ['arborist'], 'package dir match')
  })

  await t.test('completion to folder - invalid dir', async t => {
    const { install } = await mockComp(t, { noChdir: true })
    const res = await install.completion({ partialWord: '/does/not/exist' })
    t.strictSame(res, [], 'invalid dir: no matching')
  })

  await t.test('completion to folder - no matches', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './pa' })
    t.strictSame(res, [], 'no name match')
  })

  await t.test('completion to folder - match is not a package', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './othe' })
    t.strictSame(res, [], 'no name match')
  })

  await t.test('completion to url', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: 'http://path/to/url' })
    t.strictSame(res, [])
  })

  await t.test('no /', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: 'toto' })
    t.notOk(res)
  })

  await t.test('only /', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: '/' })
    t.strictSame(res, [])
  })
})

t.test('should install in workspace with unhoisted module', async t => {
  const { npm, registry, assert } = await loadMockNpm(t, {
    prefixDir: workspaceMock(t, {
      clean: true,
      workspaces: {
        'workspace-a': {
          'abbrev@1.1.0': { hoist: true },
        },
        'workspace-b': {
          'abbrev@1.1.1': { hoist: false },
        },
      },
    }),
  })
  await registry.setup({
    'abbrev@1.1.0': path.join(npm.prefix, 'tarballs/abbrev@1.1.0'),
    'abbrev@1.1.1': path.join(npm.prefix, 'tarballs/abbrev@1.1.1'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  assert.packageMissing('node_modules/abbrev@1.1.0')
  assert.packageMissing('workspace-b/node_modules/abbrev@1.1.1')
  await npm.exec('install', [])
  assert.packageInstalled('node_modules/abbrev@1.1.0')
  assert.packageInstalled('workspace-b/node_modules/abbrev@1.1.1')
})

t.test('should install in workspace with hoisted modules', async t => {
  const prefixDir = workspaceMock(t, {
    clean: true,
    workspaces: {
      'workspace-a': {
        'abbrev@1.1.0': { hoist: true },
      },
      'workspace-b': {
        'lodash@1.1.1': { hoist: true },
      },
    },
  })
  const { npm, registry, assert } = await loadMockNpm(t, { prefixDir })
  await registry.setup({
    'abbrev@1.1.0': path.join(npm.prefix, 'tarballs/abbrev@1.1.0'),
    'lodash@1.1.1': path.join(npm.prefix, 'tarballs/lodash@1.1.1'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  assert.packageMissing('node_modules/abbrev@1.1.0')
  assert.packageMissing('node_modules/lodash@1.1.1')
  await npm.exec('install', [])
  assert.packageInstalled('node_modules/abbrev@1.1.0')
  assert.packageInstalled('node_modules/lodash@1.1.1')
})

t.test('should install unhoisted module with --workspace flag', async t => {
  const { npm, registry, assert } = await loadMockNpm(t, {
    config: {
      workspace: 'workspace-b',
    },
    prefixDir: workspaceMock(t, {
      clean: true,
      workspaces: {
        'workspace-a': {
          'abbrev@1.1.0': { hoist: true },
        },
        'workspace-b': {
          'abbrev@1.1.1': { hoist: false },
        },
      },
    }),
  })
  await registry.setup({
    'abbrev@1.1.1': path.join(npm.prefix, 'tarballs/abbrev@1.1.1'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  assert.packageMissing('node_modules/abbrev@1.1.0')
  assert.packageMissing('workspace-b/node_modules/abbrev@1.1.1')
  await npm.exec('install', [])
  assert.packageMissing('node_modules/abbrev@1.1.0')
  assert.packageInstalled('workspace-b/node_modules/abbrev@1.1.1')
})

t.test('should install hoisted module with --workspace flag', async t => {
  const { npm, registry, assert } = await loadMockNpm(t, {
    config: {
      workspace: 'workspace-b',
    },
    prefixDir: workspaceMock(t, {
      clean: true,
      workspaces: {
        'workspace-a': {
          'abbrev@1.1.0': { hoist: true },
        },
        'workspace-b': {
          'lodash@1.1.1': { hoist: true },
        },
      },
    }),
  })
  await registry.setup({
    'lodash@1.1.1': path.join(npm.prefix, 'tarballs/lodash@1.1.1'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  assert.packageMissing('node_modules/abbrev@1.1.0')
  assert.packageMissing('node_modules/lodash@1.1.1')
  await npm.exec('install', [])
  assert.packageMissing('node_modules/abbrev@1.1.0')
  assert.packageInstalled('node_modules/lodash@1.1.1')
})

t.test('should show install keeps dirty --workspace flag', async t => {
  const { npm, registry, assert } = await loadMockNpm(t, {
    config: {
      workspace: 'workspace-b',
    },
    prefixDir: workspaceMock(t, {
      workspaces: {
        'workspace-a': {
          'abbrev@1.1.0': { clean: false, hoist: true },
        },
        'workspace-b': {
          'lodash@1.1.1': { clean: true, hoist: true },
        },
      },
    }),
  })
  await registry.setup({
    'lodash@1.1.1': path.join(npm.prefix, 'tarballs/lodash@1.1.1'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  assert.packageDirty('node_modules/abbrev@1.1.0')
  assert.packageMissing('node_modules/lodash@1.1.1')
  await npm.exec('install', [])
  assert.packageDirty('node_modules/abbrev@1.1.0')
  assert.packageInstalled('node_modules/lodash@1.1.1')
})

t.test('devEngines', async t => {
  const mockArguments = {
    globals: {
      'process.platform': 'linux',
      'process.arch': 'x86',
      'process.version': 'v1337.0.0',
    },
    mocks: {
      '{ROOT}/package.json': { version: '42.0.0' },
    },
  }

  t.test('should utilize devEngines success case', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'test-package',
          version: '1.0.0',
          devEngines: {
            runtime: {
              name: 'node',
            },
          },
        }),
      },
    })
    await npm.exec('install', [])
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(!output.includes('EBADDEVENGINES'))
  })

  t.test('should utilize devEngines failure case', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'test-package',
          version: '1.0.0',
          devEngines: {
            runtime: {
              name: 'nondescript',
            },
          },
        }),
      },
    })
    await t.rejects(
      npm.exec('install', [])
    )
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(output.includes('error EBADDEVENGINES'))
  })

  t.test('should utilize devEngines failure force case', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      config: {
        force: true,
      },
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'test-package',
          version: '1.0.0',
          devEngines: {
            runtime: {
              name: 'nondescript',
            },
          },
        }),
      },
    })
    await npm.exec('install', [])
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(output.includes('warn EBADDEVENGINES'))
  })

  t.test('should utilize devEngines 2x warning case', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'test-package',
          version: '1.0.0',
          devEngines: {
            runtime: {
              name: 'nondescript',
              onFail: 'warn',
            },
            cpu: {
              name: 'risv',
              onFail: 'warn',
            },
          },
        }),
      },
    })
    await npm.exec('install', [])
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(output.includes('warn EBADDEVENGINES'))
  })

  t.test('should utilize devEngines 2x error case', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'test-package',
          version: '1.0.0',
          devEngines: {
            runtime: {
              name: 'nondescript',
              onFail: 'error',
            },
            cpu: {
              name: 'risv',
              onFail: 'error',
            },
          },
        }),
      },
    })
    await t.rejects(
      npm.exec('install', [])
    )
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(output.includes('error EBADDEVENGINES'))
  })

  t.test('should utilize devEngines failure and warning case', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'test-package',
          version: '1.0.0',
          devEngines: {
            runtime: {
              name: 'nondescript',
            },
            cpu: {
              name: 'risv',
              onFail: 'warn',
            },
          },
        }),
      },
    })
    await t.rejects(
      npm.exec('install', [])
    )
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(output.includes('EBADDEVENGINES'))
  })

  t.test('should show devEngines has no effect on package install', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        alpha: {
          'package.json': JSON.stringify({
            name: 'alpha',
            devEngines: { runtime: { name: 'node', version: '1.0.0' } },
          }),
          'index.js': 'console.log("this is alpha index")',
        },
        'package.json': JSON.stringify({
          name: 'project',
        }),
      },
    })
    await npm.exec('install', ['./alpha'])
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(!output.includes('EBADDEVENGINES'))
  })

  t.test('should show devEngines has no effect on dev package install', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        alpha: {
          'package.json': JSON.stringify({
            name: 'alpha',
            devEngines: { runtime: { name: 'node', version: '1.0.0' } },
          }),
          'index.js': 'console.log("this is alpha index")',
        },
        'package.json': JSON.stringify({
          name: 'project',
        }),
      },
      config: {
        'save-dev': true,
      },
    })
    await npm.exec('install', ['./alpha'])
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(!output.includes('EBADDEVENGINES'))
  })

  t.test('should show devEngines doesnt break engines', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        alpha: {
          'package.json': JSON.stringify({
            name: 'alpha',
            devEngines: { runtime: { name: 'node', version: '1.0.0' } },
            engines: { node: '1.0.0' },
          }),
          'index.js': 'console.log("this is alpha index")',
        },
        'package.json': JSON.stringify({
          name: 'project',
        }),
      },
      config: { global: true },
    })
    await npm.exec('install', ['./alpha'])
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(output.includes('warn EBADENGINE'))
  })

  t.test('should not utilize engines in root if devEngines is provided', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'alpha',
          engines: {
            node: '0.0.1',
          },
          devEngines: {
            runtime: {
              name: 'node',
              version: '0.0.1',
              onFail: 'warn',
            },
          },
        }),
        'index.js': 'console.log("this is alpha index")',
      },
    })
    await npm.exec('install')
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(!output.includes('EBADENGINE'))
    t.ok(output.includes('warn EBADDEVENGINES'))
  })

  t.test('should utilize engines in root if devEngines is not provided', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'alpha',
          engines: {
            node: '0.0.1',
          },
        }),
        'index.js': 'console.log("this is alpha index")',
      },
    })
    await npm.exec('install')
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(output.includes('EBADENGINE'))
    t.ok(!output.includes('EBADDEVENGINES'))
  })

  t.test('should show devEngines has no effect on global package install', async t => {
    const { npm, joinedFullOutput } = await loadMockNpm(t, {
      ...mockArguments,
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'alpha',
          bin: {
            alpha: 'index.js',
          },
          devEngines: {
            runtime: {
              name: 'node',
              version: '0.0.1',
            },
          },
        }),
        'index.js': 'console.log("this is alpha index")',
      },
      config: {
        global: true,
      },
    })
    await npm.exec('install', ['.'])
    const output = joinedFullOutput()
    t.matchSnapshot(output)
    t.ok(!output.includes('EBADENGINE'))
    t.ok(!output.includes('EBADDEVENGINES'))
  })
})

// Issue #8726 - npm install should re-resolve to satisfy peerOptional constraints
// https://github.com/npm/cli/issues/8726
//
// When a lockfile has fetcher@1.1.0 but a peerOptional wants fetcher@1.0.0 (exact), npm install (save: true) should re-resolve fetcher to 1.0.0 to satisfy both the regular dep range (^1.0.0) and the exact peerOptional constraint.
t.test('issue-8726: npm install re-resolves to satisfy peerOptional constraint', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { audit: false, 'ignore-scripts': true },
    prefixDir: {
      'linter-tarball': {
        'package.json': JSON.stringify({
          name: 'linter',
          version: '1.0.0',
          dependencies: { scanner: '1.0.0' },
        }),
      },
      'scanner-tarball': {
        'package.json': JSON.stringify({
          name: 'scanner',
          version: '1.0.0',
          peerDependencies: { fetcher: '1.0.0' },
          peerDependenciesMeta: { fetcher: { optional: true } },
        }),
      },
      'hint-tarball': {
        'package.json': JSON.stringify({
          name: 'hint',
          version: '1.0.0',
          dependencies: { fetcher: '^1.0.0' },
        }),
      },
      'fetcher-1.0.0-tarball': {
        'package.json': JSON.stringify({ name: 'fetcher', version: '1.0.0' }),
      },
      'fetcher-1.1.0-tarball': {
        'package.json': JSON.stringify({ name: 'fetcher', version: '1.1.0' }),
      },
      'package.json': JSON.stringify({
        name: 'test-package',
        version: '1.0.0',
        devDependencies: {
          linter: '1.0.0',
          hint: '1.0.0',
        },
      }),
      'package-lock.json': JSON.stringify({
        name: 'test-package',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': {
            name: 'test-package',
            version: '1.0.0',
            devDependencies: { linter: '1.0.0', hint: '1.0.0' },
          },
          'node_modules/linter': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/linter/-/linter-1.0.0.tgz',
            dev: true,
            dependencies: { scanner: '1.0.0' },
          },
          'node_modules/scanner': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/scanner/-/scanner-1.0.0.tgz',
            dev: true,
            peerDependencies: { fetcher: '1.0.0' },
            peerDependenciesMeta: { fetcher: { optional: true } },
          },
          'node_modules/hint': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/hint/-/hint-1.0.0.tgz',
            dev: true,
            dependencies: { fetcher: '^1.0.0' },
          },
          'node_modules/fetcher': {
            version: '1.1.0',
            resolved: 'https://registry.npmjs.org/fetcher/-/fetcher-1.1.0.tgz',
            dev: true,
          },
        },
      }),
    },
  })

  // Only set up mocks that npm install actually needs: tarballs for all installed packages (linter, scanner, hint, fetcher@1.0.0) and the fetcher packument (needed for re-resolution via #problemEdges).
  // Packuments for linter/scanner/hint are NOT needed (already in lockfile).
  // Fetcher@1.1.0 tarball is NOT needed (gets replaced by 1.0.0).
  const linterManifest = registry.manifest({ name: 'linter' })
  await registry.tarball({
    manifest: linterManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'linter-tarball'),
  })

  const scannerManifest = registry.manifest({ name: 'scanner' })
  await registry.tarball({
    manifest: scannerManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'scanner-tarball'),
  })

  const hintManifest = registry.manifest({ name: 'hint' })
  await registry.tarball({
    manifest: hintManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'hint-tarball'),
  })

  const fetcherManifest = registry.manifest({
    name: 'fetcher',
    versions: ['1.0.0', '1.1.0'],
  })
  await registry.package({ manifest: fetcherManifest })
  await registry.tarball({
    manifest: fetcherManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'fetcher-1.0.0-tarball'),
  })

  await npm.exec('install', [])

  // Read the updated lockfile and verify fetcher was re-resolved to 1.0.0
  const lockfile = JSON.parse(
    fs.readFileSync(path.join(npm.prefix, 'package-lock.json'), 'utf8')
  )
  t.equal(
    lockfile.packages['node_modules/fetcher'].version,
    '1.0.0',
    'lockfile updated fetcher to satisfy peerOptional constraint'
  )

  // Also verify the installed package
  const installedFetcher = JSON.parse(
    fs.readFileSync(
      path.join(npm.prefix, 'node_modules', 'fetcher', 'package.json'), 'utf8'
    )
  )
  t.equal(
    installedFetcher.version,
    '1.0.0',
    'installed fetcher version satisfies peerOptional constraint'
  )
})

// Issue #8726 - fresh npm install (no lockfile) should pick a version that satisfies both the regular dep range AND the exact peerOptional constraint, even when the peerOptional holder is processed BEFORE the dep is placed.
// https://github.com/npm/cli/issues/8726
//
// This test uses package names that reproduce the real-world alphabetical ordering from the original issue (addons-linter < htmlhint), which causes addons-scanner to be processed from the queue BEFORE htmlhint places node-fetcher.
// At that point the peerOptional edge has no destination (MISSING, valid for peerOptional).
// Later, htmlhint places node-fetcher@1.1.0 and the edge becomes INVALID.
// The fix re-queues addons-scanner so #problemEdges can trigger re-resolution of node-fetcher to 1.0.0.
//
// Dependency graph:
//   root -> addons-linter@1.0.0 -> addons-scanner@1.0.0 -> PEER_OPTIONAL node-fetcher@1.0.0
//   root -> htmlhint@1.0.0      -> node-fetcher@^1.0.0
//
// Processing order (alphabetical): addons-linter, then addons-scanner (dep of addons-linter), THEN htmlhint (which places node-fetcher@1.1.0)
t.test('issue-8726: fresh install re-queues scanner when dep placed later', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { audit: false, 'ignore-scripts': true },
    prefixDir: {
      'addons-linter-tarball': {
        'package.json': JSON.stringify({
          name: 'addons-linter',
          version: '1.0.0',
          dependencies: { 'addons-scanner': '1.0.0' },
        }),
      },
      'addons-scanner-tarball': {
        'package.json': JSON.stringify({
          name: 'addons-scanner',
          version: '1.0.0',
          peerDependencies: { 'node-fetcher': '1.0.0' },
          peerDependenciesMeta: { 'node-fetcher': { optional: true } },
        }),
      },
      'htmlhint-tarball': {
        'package.json': JSON.stringify({
          name: 'htmlhint',
          version: '1.0.0',
          dependencies: { 'node-fetcher': '^1.0.0' },
        }),
      },
      'node-fetcher-1.0.0-tarball': {
        'package.json': JSON.stringify({ name: 'node-fetcher', version: '1.0.0' }),
      },
      'node-fetcher-1.1.0-tarball': {
        'package.json': JSON.stringify({ name: 'node-fetcher', version: '1.1.0' }),
      },
      'package.json': JSON.stringify({
        name: 'test-package',
        version: '1.0.0',
        devDependencies: {
          'addons-linter': '1.0.0',
          htmlhint: '1.0.0',
        },
      }),
      // NO package-lock.json — this is a fresh install
    },
  })

  // Fresh install needs packuments for all packages
  const linterManifest = registry.manifest({
    name: 'addons-linter',
    packuments: [{ version: '1.0.0', dependencies: { 'addons-scanner': '1.0.0' } }],
  })
  await registry.package({ manifest: linterManifest })
  await registry.tarball({
    manifest: linterManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'addons-linter-tarball'),
  })

  const scannerManifest = registry.manifest({
    name: 'addons-scanner',
    packuments: [{
      version: '1.0.0',
      peerDependencies: { 'node-fetcher': '1.0.0' },
      peerDependenciesMeta: { 'node-fetcher': { optional: true } },
    }],
  })
  await registry.package({ manifest: scannerManifest })
  await registry.tarball({
    manifest: scannerManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'addons-scanner-tarball'),
  })

  const hintManifest = registry.manifest({
    name: 'htmlhint',
    packuments: [{ version: '1.0.0', dependencies: { 'node-fetcher': '^1.0.0' } }],
  })
  await registry.package({ manifest: hintManifest })
  await registry.tarball({
    manifest: hintManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'htmlhint-tarball'),
  })

  const fetcherManifest = registry.manifest({
    name: 'node-fetcher',
    packuments: [{ version: '1.0.0' }, { version: '1.1.0' }],
  })
  // Packument is fetched twice: once when htmlhint resolves node-fetcher@^1.0.0 (picking 1.1.0), and again when addons-scanner is re-queued and re-resolves node-fetcher (picking 1.0.0 to satisfy the exact peerOptional spec).
  await registry.package({ manifest: fetcherManifest, times: 2 })
  await registry.tarball({
    manifest: fetcherManifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'node-fetcher-1.0.0-tarball'),
  })
  // node-fetcher@1.1.0 tarball is NOT needed: it's replaced by 1.0.0 during tree building (before reification), so it's never downloaded.

  await npm.exec('install', [])

  // Verify the lockfile has node-fetcher@1.0.0
  const lockfile = JSON.parse(
    fs.readFileSync(path.join(npm.prefix, 'package-lock.json'), 'utf8')
  )
  t.equal(
    lockfile.packages['node_modules/node-fetcher'].version,
    '1.0.0',
    'fresh install picks node-fetcher@1.0.0 satisfying peerOptional constraint'
  )

  // Also verify the installed package
  const installedFetcher = JSON.parse(
    fs.readFileSync(
      path.join(npm.prefix, 'node_modules', 'node-fetcher', 'package.json'), 'utf8'
    )
  )
  t.equal(
    installedFetcher.version,
    '1.0.0',
    'installed node-fetcher version satisfies peerOptional constraint'
  )
})
