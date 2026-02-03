const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

const cacache = require('cacache')
const fs = require('node:fs')
const path = require('node:path')
const { cleanCwd } = require('../../fixtures/clean-snapshot.js')

const pkg = 'test-package'

const createNpxCacheEntry = (npxCacheDir, hash, pkgJson, shrinkwrapJson) => {
  fs.mkdirSync(path.join(npxCacheDir, hash))
  fs.writeFileSync(
    path.join(npxCacheDir, hash, 'package.json'),
    JSON.stringify(pkgJson)
  )
  if (shrinkwrapJson) {
    fs.writeFileSync(
      path.join(npxCacheDir, hash, 'npm-shrinkwrap.json'),
      JSON.stringify(shrinkwrapJson)
    )
  }
}

t.cleanSnapshot = str => {
  return cleanCwd(str)
    .replace(/Finished in [0-9.s]+/g, 'Finished in xxxs')
    .replace(/Cache verified and compressed (.*)/, 'Cache verified and compressed ({PATH})')
}

t.test('cache no args', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('cache', []),
    { code: 'EUSAGE' },
    'should throw usage instructions'
  )
})

t.test('cache clean', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('cache', ['clean']),
    /the npm cache self-heals/,
    'should throw warning'
  )
})

t.test('cache clean (force)', async t => {
  const { npm } = await loadMockNpm(t, {
    cacheDir: { _cacache: {} },
    config: { force: true },
  })
  const cache = path.join(npm.cache, '_cacache')
  await npm.exec('cache', ['clean'])
  t.notOk(fs.existsSync(cache), 'cache dir was removed')
})

t.test('cache add no arg', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('cache', ['add']),
    {
      code: 'EUSAGE',
      message: 'First argument to `add` is required',
    },
    'throws usage error'
  )
})

t.test('cache add single pkg', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: {
      package: {
        'package.json': JSON.stringify({
          name: pkg,
          version: '1.0.0',
        }),
      },
    },
  })
  const cache = path.join(npm.cache, '_cacache')
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  const manifest = registry.manifest({ name: pkg })
  await registry.package({
    manifest,
    times: 2,
    tarballs: { '1.0.0': path.join(npm.prefix, 'package') },
  })
  await npm.exec('cache', ['add', pkg])
  t.equal(joinedOutput(), '')
  t.resolves(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz'))
  t.resolves(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package'))
})

t.test('cache add multiple pkgs', async t => {
  const pkg2 = 'test-package-two'
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: {
      package: {
        'package.json': JSON.stringify({
          name: pkg,
          version: '1.0.0',
        }),
      },
    },
  })
  const cache = path.join(npm.cache, '_cacache')
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  const manifest = registry.manifest({ name: pkg })
  const manifest2 = registry.manifest({ name: pkg2 })
  await registry.package({
    manifest,
    times: 2,
    tarballs: { '1.0.0': path.join(npm.prefix, 'package') },
  })
  await registry.package({
    manifest: manifest2, times: 2, tarballs: { '1.0.0': path.join(npm.prefix, 'package') },
  })
  await npm.exec('cache', ['add', pkg, pkg2])
  t.equal(joinedOutput(), '')
  t.resolves(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz'))
  t.resolves(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package'))
  t.resolves(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package-two/-/test-package-two-1.0.0.tgz'))
  t.resolves(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package-two'))
})

t.test('cache ls', async t => {
  const keys = [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz',
  ]
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  for (const key of keys) {
    await cacache.put(cache, key, 'test data')
  }
  await npm.exec('cache', ['ls'])
  t.matchSnapshot(joinedOutput(), 'logs cache entries')
})

t.test('cache ls pkgs', async t => {
  const keys = [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/npm',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/npm/-/npm-1.2.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.47.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.40.0.tgz',
  ]
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  for (const key of keys) {
    await cacache.put(cache, key, 'test data')
  }
  await cacache.put(cache,
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack',
    JSON.stringify({ versions: {
      '4.40.0': { dist: { tarball: 'https://registry.npmjs.org/webpack/-/webpack-4.40.0.tgz' } },
      '4.47.0': { dist: { tarball: 'https://registry.npmjs.org/webpack/-/webpack-4.47.0.tgz' } },
    } })
  )
  await npm.exec('cache', ['ls', 'webpack@>4.44.1', 'npm'])
  t.matchSnapshot(joinedOutput(), 'logs cache entries for npm and webpack and one webpack tgz')
})

t.test('cache ls special', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  await cacache.put(cache,
    'make-fetch-happen:request-cache:https://registry.npmjs.org/foo',
    JSON.stringify({ versions: { '1.2.3-beta': {} } })
  )
  await cacache.put(cache,
    'make-fetch-happen:request-cache:https://registry.npmjs.org/foo/-/foo-1.2.3-beta.tgz',
    'test-data'
  )
  await npm.exec('cache', ['ls', 'foo@1.2.3-beta'])
  t.matchSnapshot(joinedOutput(), 'logs cache entries for foo')
})

t.test('cache ls nonpublic registry', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  await cacache.put(cache,
    'make-fetch-happen:request-cache:https://somerepo.github.org/extemporaneously',
    JSON.stringify({
      versions: { '1.0.0': { dist: { tarball: 'https://somerepo.github.org/aabbcc/' } } },
    })
  )
  await cacache.put(cache,
    'make-fetch-happen:request-cache:https://somerepo.github.org/aabbcc/',
    'test data'
  )
  await npm.exec('cache', ['ls', 'extemporaneously'])
  t.matchSnapshot(joinedOutput(), 'logs cache entry for extemporaneously and its tarball')
})

t.test('cache ls tagged', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('cache', ['ls', 'webpack@latest']),
    { code: 'EUSAGE' },
    'should throw usage error'
  )
})

t.test('cache ls scoped and scoped slash', async t => {
  const keys = [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy/staydown/-/@fritzy/staydown-3.1.1.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy%2fstaydown',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar/npm-expansion/-/@gar/npm-expansion-2.1.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar%2fnpm-expansion',
  ]
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  for (const key of keys) {
    await cacache.put(cache, key, 'test data')
  }
  await npm.exec('cache', ['ls', '@fritzy/staydown', '@gar/npm-expansion'])
  t.matchSnapshot(joinedOutput(), 'logs cache entries for @gar and @fritzy')
})

t.test('cache ls corrupted', async t => {
  const keys = [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted/-/corrupted-3.1.0.tgz',
  ]
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  for (const key of keys) {
    await cacache.put(cache, key, Buffer.from('<>>>}"'))
  }
  await npm.exec('cache', ['ls', 'corrupted'])
  t.matchSnapshot(joinedOutput(), 'logs cache entries with bad data')
})

t.test('cache ls missing packument version not an object', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  await cacache.put(cache,
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version',
    JSON.stringify({ versions: 'not an object' })
  )
  await npm.exec('cache', ['ls', 'missing-version'])
  t.matchSnapshot(joinedOutput(), 'logs cache entry for packument')
})

t.test('cache rm', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const cache = path.join(npm.cache, '_cacache')
  await cacache.put(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package', '{}')
  await cacache.put(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz', 'test data')
  await npm.exec('cache', ['rm', 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz'])
  t.matchSnapshot(joinedOutput(), 'logs deleting single entry')
  t.resolves(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package'))
  t.rejects(cacache.get(cache, 'make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz'))
})

t.test('cache rm unfound', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  await npm.exec('cache', ['rm', 'made-up-key'])
  t.same(joinedOutput(), '', 'no errors, no output')
})

t.test('cache verify', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  await npm.exec('cache', ['verify'])
  t.matchSnapshot(joinedOutput(), 'shows verified cache output')
})

t.test('cache verify as part of home', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    globals: ({ prefix }) => ({ 'process.env.HOME': path.dirname(prefix) }),
  })
  await npm.exec('cache', ['verify'])
  t.match(joinedOutput(), 'Cache verified and compressed (~', 'contains ~ shorthand')
})

t.test('cache verify w/ extra output', async t => {
  const verify = {
    runTime: {
      markStartTime: 0,
      fixPerms: 3,
      garbageCollect: 54982,
      rebuildIndex: 62779,
      cleanTmp: 62781,
      writeVerifile: 62783,
      markEndTime: 62783,
      total: 62783,
    },
    verifiedContent: 17057,
    reclaimedCount: 1144,
    reclaimedSize: 248164665,
    badContentCount: 12345,
    keptSize: 1644485260,
    missingContent: 92,
    rejectedEntries: 92,
    totalEntries: 20175,
  }
  const { npm, joinedOutput } = await loadMockNpm(t, {
    mocks: { cacache: { verify: () => verify } },
  })
  await npm.exec('cache', ['verify'])
  t.matchSnapshot(joinedOutput(), 'shows extra output')
})

t.test('cache completion', async t => {
  const { cache } = await loadMockNpm(t, { command: 'cache' })
  const { completion } = cache

  const testComp = (argv, expect) => {
    return t.resolveMatch(completion({ conf: { argv: { remain: argv } } }), expect, argv.join(' '))
  }

  await Promise.all([
    testComp(['npm', 'cache'], ['add', 'clean', 'verify']),
    testComp(['npm', 'cache', 'add'], []),
    testComp(['npm', 'cache', 'clean'], []),
    testComp(['npm', 'cache', 'verify'], []),
  ])
})

t.test('cache npx ls: empty cache', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  await npm.exec('cache', ['npx', 'ls'])
  t.matchSnapshot(joinedOutput(), 'logs message for empty npx cache')
})

t.test('cache npx ls: some entries', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  // Make two fake entries: one valid, one invalid
  const hash1 = 'abc123'
  const hash2 = 'z9y8x7'
  fs.mkdirSync(path.join(npxCacheDir, hash1))
  fs.writeFileSync(
    path.join(npxCacheDir, hash1, 'package.json'),
    JSON.stringify({
      name: 'fake-npx-package',
      version: '1.0.0',
      _npx: { packages: ['fake-npx-package@1.0.0'] },
    })
  )
  // invalid (missing or broken package.json) directory
  fs.mkdirSync(path.join(npxCacheDir, hash2))

  await npm.exec('cache', ['npx', 'ls'])
  t.matchSnapshot(joinedOutput(), 'lists one valid and one invalid entry')
})

t.test('cache npx info: valid and invalid entry', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const goodHash = 'deadbeef'
  fs.mkdirSync(path.join(npxCacheDir, goodHash))
  fs.writeFileSync(
    path.join(npxCacheDir, goodHash, 'package.json'),
    JSON.stringify({
      name: 'good-npx-package',
      version: '2.0.0',
      dependencies: {
        rimraf: '^3.0.0',
      },
      _npx: { packages: ['good-npx-package@2.0.0'] },
    })
  )

  const badHash = 'badc0de'
  fs.mkdirSync(path.join(npxCacheDir, badHash))

  await npm.exec('cache', ['npx', 'info', goodHash])
  t.matchSnapshot(joinedOutput(), 'shows valid package info')

  await npm.exec('cache', ['npx', 'info', badHash])
  t.matchSnapshot(joinedOutput(), 'shows invalid package info')
})

t.test('cache npx rm: remove single entry', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const removableHash = '123removeme'
  fs.mkdirSync(path.join(npxCacheDir, removableHash))
  fs.writeFileSync(
    path.join(npxCacheDir, removableHash, 'package.json'),
    JSON.stringify({ name: 'removable-package', _npx: { packages: ['removable-package@1.0.0'] } })
  )

  npm.config.set('dry-run', true)
  await npm.exec('cache', ['npx', 'rm', removableHash])
  t.ok(fs.existsSync(path.join(npxCacheDir, removableHash)), 'entry folder remains')
  npm.config.set('dry-run', false)

  await npm.exec('cache', ['npx', 'rm', removableHash])
  t.matchSnapshot(joinedOutput(), 'logs removing single npx cache entry')
  t.notOk(fs.existsSync(path.join(npxCacheDir, removableHash)), 'entry folder removed')
})

t.test('cache npx rm: removing all without --force fails', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const testHash = 'remove-all-no-force'
  fs.mkdirSync(path.join(npxCacheDir, testHash))
  fs.writeFileSync(
    path.join(npxCacheDir, testHash, 'package.json'),
    JSON.stringify({ name: 'no-force-pkg', _npx: { packages: ['no-force-pkg@1.0.0'] } })
  )

  await t.rejects(
    npm.exec('cache', ['npx', 'rm']),
    /Please use --force to remove entire npx cache/,
    'fails without --force'
  )
  t.matchSnapshot(joinedOutput(), 'logs usage error when removing all without --force')

  t.ok(fs.existsSync(path.join(npxCacheDir, testHash)), 'folder still exists')
})

t.test('cache npx rm: removing all with --force works', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { force: true },
  })
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const testHash = 'remove-all-yes-force'
  fs.mkdirSync(path.join(npxCacheDir, testHash))
  fs.writeFileSync(
    path.join(npxCacheDir, testHash, 'package.json'),
    JSON.stringify({ name: 'yes-force-pkg', _npx: { packages: ['yes-force-pkg@1.0.0'] } })
  )

  npm.config.set('dry-run', true)
  await npm.exec('cache', ['npx', 'rm'])
  t.ok(fs.existsSync(npxCacheDir), 'npx cache directory remains')
  npm.config.set('dry-run', false)

  await npm.exec('cache', ['npx', 'rm'])

  t.matchSnapshot(joinedOutput(), 'logs removing everything')
  t.notOk(fs.existsSync(npxCacheDir), 'npx cache directory removed')
})

t.test('cache npx rm: removing more than 1, less than all entries', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  // Removable folder
  const removableHash = '123removeme'
  fs.mkdirSync(path.join(npxCacheDir, removableHash))
  fs.writeFileSync(
    path.join(npxCacheDir, removableHash, 'package.json'),
    JSON.stringify({ name: 'removable-package', _npx: { packages: ['removable-package@1.0.0'] } })
  )

  // Another Removable folder
  const anotherRemovableHash = '456removeme'
  fs.mkdirSync(path.join(npxCacheDir, anotherRemovableHash))
  fs.writeFileSync(
    path.join(npxCacheDir, anotherRemovableHash, 'package.json'),
    JSON.stringify({ name: 'another-removable-package', _npx: { packages: ['another-removable-package@1.0.0'] } })
  )

  // Another folder that should remain
  const keepHash = '999keep'
  fs.mkdirSync(path.join(npxCacheDir, keepHash))
  fs.writeFileSync(
    path.join(npxCacheDir, keepHash, 'package.json'),
    JSON.stringify({ name: 'keep-package', _npx: { packages: ['keep-package@1.0.0'] } })
  )

  npm.config.set('dry-run', true)
  await npm.exec('cache', ['npx', 'rm', removableHash, anotherRemovableHash])
  t.ok(fs.existsSync(path.join(npxCacheDir, removableHash)), 'entry folder remains')
  t.ok(fs.existsSync(path.join(npxCacheDir, anotherRemovableHash)), 'entry folder remains')
  t.ok(fs.existsSync(path.join(npxCacheDir, keepHash)), 'entry folder remains')
  npm.config.set('dry-run', false)

  await npm.exec('cache', ['npx', 'rm', removableHash, anotherRemovableHash])
  t.matchSnapshot(joinedOutput(), 'logs removing 2 of 3 entries')

  t.notOk(fs.existsSync(path.join(npxCacheDir, removableHash)), 'removed folder no longer exists')
  t.notOk(fs.existsSync(path.join(npxCacheDir, anotherRemovableHash)), 'the other folder no longer exists')
  t.ok(fs.existsSync(path.join(npxCacheDir, keepHash)), 'the other folder remains')
})

t.test('cache npx should throw usage error', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('cache', ['npx', 'badcommand']),
    { code: 'EUSAGE' },
    'should throw usage error'
  )
})

t.test('cache npx should throw usage error for invalid key', async t => {
  const { npm } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const key = 'badkey'
  await t.rejects(
    npm.exec('cache', ['npx', 'rm', key]),
    { code: 'EUSAGE' },
    `Invalid npx key ${key}`
  )
})

t.test('cache npx ls: entry with unknown package', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  // Create an entry without the _npx property
  const unknownHash = 'unknown123'
  fs.mkdirSync(path.join(npxCacheDir, unknownHash))
  fs.writeFileSync(
    path.join(npxCacheDir, unknownHash, 'package.json'),
    JSON.stringify({
      name: 'unknown-package',
      version: '1.0.0',
    })
  )

  await npm.exec('cache', ['npx', 'ls'])
  t.matchSnapshot(joinedOutput(), 'lists entry with unknown package')
})

t.test('cache npx info: should throw usage error when no keys are provided', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('cache', ['npx', 'info']),
    { code: 'EUSAGE' },
    'should throw usage error when no keys are provided'
  )
})

t.test('cache npx info: valid entry with _npx packages', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const validHash = 'valid123'
  createNpxCacheEntry(npxCacheDir, validHash, {
    name: 'valid-package',
    version: '1.0.0',
    _npx: { packages: ['valid-package@1.0.0'] },
  }, {
    name: 'valid-package',
    version: '1.0.0',
    dependencies: {
      'valid-package': {
        version: '1.0.0',
        resolved: 'https://registry.npmjs.org/valid-package/-/valid-package-1.0.0.tgz',
        integrity: 'sha512-...',
      },
    },
  })

  const nodeModulesDir = path.join(npxCacheDir, validHash, 'node_modules')
  fs.mkdirSync(nodeModulesDir, { recursive: true })
  fs.mkdirSync(path.join(nodeModulesDir, 'valid-package'))
  fs.writeFileSync(
    path.join(nodeModulesDir, 'valid-package', 'package.json'),
    JSON.stringify({
      name: 'valid-package',
      version: '1.0.0',
    })
  )

  await npm.exec('cache', ['npx', 'info', validHash])
  t.matchSnapshot(joinedOutput(), 'shows valid package info with _npx packages')
})

t.test('cache npx info: valid entry with dependencies', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const validHash = 'valid456'
  createNpxCacheEntry(npxCacheDir, validHash, {
    name: 'valid-package',
    version: '1.0.0',
    dependencies: {
      'dep-package': '1.0.0',
    },
  }, {
    name: 'valid-package',
    version: '1.0.0',
    dependencies: {
      'dep-package': {
        version: '1.0.0',
        resolved: 'https://registry.npmjs.org/dep-package/-/dep-package-1.0.0.tgz',
        integrity: 'sha512-...',
      },
    },
  })

  const nodeModulesDir = path.join(npxCacheDir, validHash, 'node_modules')
  fs.mkdirSync(nodeModulesDir, { recursive: true })
  fs.mkdirSync(path.join(nodeModulesDir, 'dep-package'))
  fs.writeFileSync(
    path.join(nodeModulesDir, 'dep-package', 'package.json'),
    JSON.stringify({
      name: 'dep-package',
      version: '1.0.0',
    })
  )

  await npm.exec('cache', ['npx', 'info', validHash])
  t.matchSnapshot(joinedOutput(), 'shows valid package info with dependencies')
})

t.test('cache npx info: valid entry with _npx directory package', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx'))
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const validHash = 'valid123'
  createNpxCacheEntry(npxCacheDir, validHash, {
    name: 'valid-package',
    version: '1.0.0',
    _npx: { packages: ['/path/to/valid-package'] },
  }, {
    name: 'valid-package',
    version: '1.0.0',
    dependencies: {
      'valid-package': {
        version: '1.0.0',
        resolved: 'https://registry.npmjs.org/valid-package/-/valid-package-1.0.0.tgz',
        integrity: 'sha512-...',
      },
    },
  })

  const nodeModulesDir = path.join(npxCacheDir, validHash, 'node_modules')
  fs.mkdirSync(nodeModulesDir, { recursive: true })
  fs.mkdirSync(path.join(nodeModulesDir, 'valid-package'))
  fs.writeFileSync(
    path.join(nodeModulesDir, 'valid-package', 'package.json'),
    JSON.stringify({
      name: 'valid-package',
      version: '1.0.0',
    })
  )

  await npm.exec('cache', ['npx', 'info', validHash])
  t.matchSnapshot(joinedOutput(), 'shows valid package info with _npx directory package')
})

t.test('cache npx info: valid entry with a link dependency', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  const npxCacheDir = path.join(
    npm.flatOptions.npxCache || path.join(npm.cache, '..', '_npx')
  )
  fs.mkdirSync(npxCacheDir, { recursive: true })

  const validHash = 'link123'
  const pkgDir = path.join(npxCacheDir, validHash)
  fs.mkdirSync(pkgDir)

  fs.writeFileSync(
    path.join(pkgDir, 'package.json'),
    JSON.stringify({
      name: 'link-package',
      version: '1.0.0',
      dependencies: {
        'linked-dep': 'file:./some-other-loc',
      },
    })
  )

  fs.writeFileSync(
    path.join(pkgDir, 'npm-shrinkwrap.json'),
    JSON.stringify({
      name: 'link-package',
      version: '1.0.0',
      dependencies: {
        'linked-dep': {
          version: 'file:../some-other-loc',
        },
      },
    })
  )

  const nodeModulesDir = path.join(pkgDir, 'node_modules')
  fs.mkdirSync(nodeModulesDir, { recursive: true })

  const linkTarget = path.join(pkgDir, 'some-other-loc')
  fs.mkdirSync(linkTarget)
  fs.writeFileSync(
    path.join(linkTarget, 'package.json'),
    JSON.stringify({ name: 'linked-dep', version: '1.0.0' })
  )

  fs.symlinkSync('../some-other-loc', path.join(nodeModulesDir, 'linked-dep'))
  await npm.exec('cache', ['npx', 'info', validHash])

  t.matchSnapshot(
    joinedOutput(),
    'shows link dependency realpath (child.isLink branch)'
  )
})
