const t = require('tap')
const { load: _loadMockNpm } = require('../../fixtures/mock-npm')
const MockRegistry = require('@npmcli/mock-registry')

const path = require('path')
const fs = require('fs')

// t.cleanSnapshot = str => str.replace(/ in [0-9ms]+/g, ' in {TIME}')

const loadMockNpm = async (t, opts) => {
  const mock = await _loadMockNpm(t, opts)
  const registry = new MockRegistry({
    tap: t,
    registry: mock.npm.config.get('registry'),
  })
  return { registry, ...mock }
}

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
    manifest: manifest.versions['1.0.0'],
    tarball: path.join(npm.prefix, 'abbrev'),
  })
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
  require('nock').emitter.on('no match', req => {
    t.fail('Should not audit')
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
        t.ok(opts.banner)
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
