const fs = require('node:fs')
const path = require('node:path')
const t = require('tap')

const {
  loadNpmWithRegistry: loadMockNpm,
  workspaceMock,
} = require('../../fixtures/mock-npm')

// t.cleanSnapshot = str => str.replace(/ in [0-9ms]+/g, ' in {TIME}')

const packageJson = {
  name: 'test-package',
  version: '1.0.0',
  dependencies: {
    abbrev: '^1.0.0',
  },
}
const packageLock = {
  name: 'test-package',
  version: '1.0.0',
  lockfileVersion: 2,
  requires: true,
  packages: {
    '': {
      name: 'test-package',
      version: '1.0.0',
      dependencies: {
        abbrev: '^1.0.0',
      },
    },
    'node_modules/abbrev': {
      version: '1.0.0',
      resolved: 'https://registry.npmjs.org/abbrev/-/abbrev-1.0.0.tgz',
      // integrity changes w/ each test cause the path is different?
    },
  },
  dependencies: {
    abbrev: {
      version: '1.0.0',
      resolved: 'https://registry.npmjs.org/abbrev/-/abbrev-1.0.0.tgz',
      // integrity changes w/ each test cause the path is different?
    },
  },
}

const abbrev = {
  'package.json': '{"name": "abbrev", "version": "1.0.0"}',
  test: 'test file',
}

t.test('reifies, but doesn\'t remove node_modules because --dry-run', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      'dry-run': true,
    },
    prefixDir: {
      abbrev: abbrev,
      'package.json': JSON.stringify(packageJson),
      'package-lock.json': JSON.stringify(packageLock),
      node_modules: { test: 'test file that will not be removed' },
    },
  })
  await npm.exec('ci', [])
  t.match(joinedOutput(), 'added 1 package, and removed 1 package in')
  const nmTest = path.join(npm.prefix, 'node_modules', 'test')
  t.equal(fs.existsSync(nmTest), true, 'existing node_modules is not removed')
  const nmAbbrev = path.join(npm.prefix, 'node_modules', 'abbrev')
  t.equal(fs.existsSync(nmAbbrev), false, 'does not install abbrev')
})

t.test('reifies, audits, removes node_modules', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    prefixDir: {
      abbrev: abbrev,
      'package.json': JSON.stringify(packageJson),
      'package-lock.json': JSON.stringify(packageLock),
      node_modules: { test: 'test file that will be removed' },
    },
  })
  const manifest = registry.manifest({ name: 'abbrev' })
  await registry.tarball({
    manifest: manifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'abbrev'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  await npm.exec('ci', [])
  t.match(joinedOutput(), 'added 1 package, and audited 2 packages in')
  const nmTest = path.join(npm.prefix, 'node_modules', 'test')
  t.equal(fs.existsSync(nmTest), false, 'existing node_modules is removed')
  const nmAbbrev = path.join(npm.prefix, 'node_modules', 'abbrev')
  t.equal(fs.existsSync(nmAbbrev), true, 'installs abbrev')
})

t.test('reifies, audits, removes node_modules on repeat run', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    prefixDir: {
      abbrev: abbrev,
      'package.json': JSON.stringify(packageJson),
      'package-lock.json': JSON.stringify(packageLock),
      node_modules: { test: 'test file that will be removed' },
    },
  })
  const manifest = registry.manifest({ name: 'abbrev' })
  await registry.tarball({
    times: 2,
    manifest: manifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'abbrev'),
  })
  await registry.tarball({
    manifest: manifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'abbrev'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  await npm.exec('ci', [])
  await npm.exec('ci', [])
  t.match(joinedOutput(), 'added 1 package, and audited 2 packages in')
  const nmTest = path.join(npm.prefix, 'node_modules', 'test')
  t.equal(fs.existsSync(nmTest), false, 'existing node_modules is removed')
  const nmAbbrev = path.join(npm.prefix, 'node_modules', 'abbrev')
  t.equal(fs.existsSync(nmAbbrev), true, 'installs abbrev')
})

t.test('--no-audit and --ignore-scripts', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    config: {
      'ignore-scripts': true,
      audit: false,
    },
    prefixDir: {
      abbrev: {
        'package.json': '{"name": "abbrev", "version": "1.0.0"}',
        test: 'test-file',
      },
      'package.json': JSON.stringify({
        ...packageJson,
        // Would make install fail
        scripts: { install: 'echo "SHOULD NOT RUN" && exit 1' },
      }),
      'package-lock.json': JSON.stringify(packageLock),
    },
  })
  const manifest = registry.manifest({ name: 'abbrev' })
  await registry.tarball({
    manifest: manifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'abbrev'),
  })
  await npm.exec('ci', [])
  t.match(joinedOutput(), 'added 1 package in', 'would fail if install script ran')
})

t.test('lifecycle scripts', async t => {
  const scripts = []
  const { npm, registry } = await loadMockNpm(t, {
    prefixDir: {
      abbrev: abbrev,
      'package.json': JSON.stringify(packageJson),
      'package-lock.json': JSON.stringify(packageLock),
    },
    mocks: {
      '@npmcli/run-script': (opts) => {
        scripts.push(opts.event)
      },
    },
  })
  const manifest = registry.manifest({ name: 'abbrev' })
  await registry.tarball({
    manifest: manifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'abbrev'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  await npm.exec('ci', [])
  t.same(scripts, [
    'preinstall',
    'install',
    'postinstall',
    'prepublish',
    'preprepare',
    'prepare',
    'postprepare',
  ], 'runs appropriate scripts, in order')
})

t.test('should throw if package-lock.json or npm-shrinkwrap missing', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify(packageJson),
      node_modules: {
        'test-file': 'should not be removed',
      },
    },
  })
  await t.rejects(npm.exec('ci', []), { code: 'EUSAGE', message: /package-lock.json/ })
  const nmTestFile = path.join(npm.prefix, 'node_modules', 'test-file')
  t.equal(fs.existsSync(nmTestFile), true, 'does not remove node_modules')
})

t.test('should throw ECIGLOBAL', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { global: true },
  })
  await t.rejects(npm.exec('ci', []), { code: 'ECIGLOBAL' })
})

t.test('should throw error when ideal inventory mismatches virtual', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    prefixDir: {
      abbrev: abbrev,
      'package.json': JSON.stringify({
        ...packageJson,
        dependencies: { notabbrev: '^1.0.0' },
      }),
      'package-lock.json': JSON.stringify(packageLock),
      node_modules: {
        'test-file': 'should not be removed',
      },
    },
  })
  const manifest = registry.manifest({ name: 'notabbrev' })
  await registry.package({ manifest })
  await t.rejects(
    npm.exec('ci', []),
    { code: 'EUSAGE', message: /in sync/ }
  )
  const nmTestFile = path.join(npm.prefix, 'node_modules', 'test-file')
  t.equal(fs.existsSync(nmTestFile), true, 'does not remove node_modules')
})

t.test('should remove dirty node_modules with unhoisted workspace module', async t => {
  const { npm, registry, assert } = await loadMockNpm(t, {
    prefixDir: workspaceMock(t, {
      clean: false,
      workspaces: {
        'workspace-a': {
          'abbrev@1.1.0': { },
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
  assert.packageDirty('node_modules/abbrev@1.1.0')
  assert.packageDirty('workspace-b/node_modules/abbrev@1.1.1')
  await npm.exec('ci', [])
  assert.packageInstalled('node_modules/abbrev@1.1.0')
  assert.packageInstalled('workspace-b/node_modules/abbrev@1.1.1')
})

t.test('should remove dirty node_modules with hoisted workspace modules', async t => {
  const { npm, registry, assert } = await loadMockNpm(t, {
    prefixDir: workspaceMock(t, {
      clean: false,
      workspaces: {
        'workspace-a': {
          'abbrev@1.1.0': { },
        },
        'workspace-b': {
          'lodash@1.1.1': { },
        },
      },
    }),
  })
  await registry.setup({
    'abbrev@1.1.0': path.join(npm.prefix, 'tarballs/abbrev@1.1.0'),
    'lodash@1.1.1': path.join(npm.prefix, 'tarballs/lodash@1.1.1'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  assert.packageDirty('node_modules/abbrev@1.1.0')
  assert.packageDirty('node_modules/lodash@1.1.1')
  await npm.exec('ci', [])
  assert.packageInstalled('node_modules/abbrev@1.1.0')
  assert.packageInstalled('node_modules/lodash@1.1.1')
})

/** this behaves the same way as install but will remove all workspace node_modules */
t.test('should use --workspace flag', async t => {
  t.saveFixture = true
  const { npm, registry, assert } = await loadMockNpm(t, {
    config: {
      workspace: 'workspace-b',
    },
    prefixDir: workspaceMock(t, {
      clean: false,
      workspaces: {
        'workspace-a': {
          'abbrev@1.1.0': { },
        },
        'workspace-b': {
          'lodash@1.1.1': { },
        },
      },
    }),
  })
  await registry.setup({
    'lodash@1.1.1': path.join(npm.prefix, 'tarballs/lodash@1.1.1'),
  })
  registry.nock.post('/-/npm/v1/security/advisories/bulk').reply(200, {})
  assert.packageDirty('node_modules/abbrev@1.1.0')
  assert.packageDirty('node_modules/lodash@1.1.1')
  await npm.exec('ci', [])
  assert.packageMissing('node_modules/abbrev@1.1.0')
  assert.packageInstalled('node_modules/lodash@1.1.1')
})

// Issue #8726 - npm ci fails because npm install produces an out-of-sync lockfile
// https://github.com/npm/cli/issues/8726
//
// Root cause: an optional peerDependency at an exact version causes buildIdealTree() to resolve a different version than what's in the lockfile.
//
// Pattern (mirrors real-world addons-linter / htmlhint / node-fetch scenario):
//   - scanner@1.0.0 has optional peerDep: fetcher@1.0.0 (exact version)
//   - hint@1.0.0 has regular dep: fetcher@^1.0.0 (semver range)
//   - npm install resolves fetcher to 1.1.0 (latest matching ^1.0.0)
//   - npm ci's buildIdealTree sees fetcher@1.1.0 doesn't satisfy the exact peerDep "1.0.0", treats it as a problem edge, resolves fetcher to 1.0.0
//   - validateLockfile: lockfile has 1.1.0, ideal tree has 1.0.0 → MISMATCH

t.test('issue-8726: npm ci with optional peerDep causing lockfile version mismatch', async t => {
  // Pre-built lockfile locks fetcher@1.1.0 (what npm install would pick).
  // scanner has optional peerDep fetcher@1.0.0 (exact version).
  // buildIdealTree should respect the lockfile version, but the bug causes it to resolve fetcher to 1.0.0, failing validateLockfile.
  const { npm, registry } = await loadMockNpm(t, {
    config: { audit: false, 'ignore-scripts': true },
    strictRegistryNock: false,
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

  // With the fix, buildIdealTree no longer treats the invalid peerOptional edge as a problem, so npm ci proceeds to reification and needs tarballs.
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
  await registry.tarball({
    manifest: fetcherManifest.versions['1.1.0'],
    tarball: path.join(npm.prefix, 'fetcher-1.1.0-tarball'),
  })

  // npm ci should succeed - the lockfile is valid and the fix ensures the peerOptional edge doesn't cause a version mismatch.
  await npm.exec('ci', [])

  const installedFetcher = JSON.parse(
    fs.readFileSync(
      path.join(npm.prefix, 'node_modules', 'fetcher', 'package.json'), 'utf8'
    )
  )
  t.equal(
    installedFetcher.version,
    '1.1.0',
    'installed the locked fetcher version, not the peer dep version'
  )
})
