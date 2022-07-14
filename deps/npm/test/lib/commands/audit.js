const t = require('tap')

const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const MockRegistry = require('../../fixtures/mock-registry.js')
const zlib = require('zlib')
const gzip = zlib.gzipSync
const gunzip = zlib.gunzipSync
const path = require('path')
const fs = require('fs')

t.cleanSnapshot = str => str.replace(/packages in [0-9]+[a-z]+/g, 'packages in xxx')

const tree = {
  'package.json': JSON.stringify({
    name: 'test-dep',
    version: '1.0.0',
    dependencies: {
      'test-dep-a': '*',
    },
  }),
  'package-lock.json': JSON.stringify({
    name: 'test-dep',
    version: '1.0.0',
    lockfileVersion: 2,
    requires: true,
    packages: {
      '': {
        xname: 'scratch',
        version: '1.0.0',
        dependencies: {
          'test-dep-a': '*',
        },
        devDependencies: {},
      },
      'node_modules/test-dep-a': {
        name: 'test-dep-a',
        version: '1.0.0',
      },
    },
    dependencies: {
      'test-dep-a': {
        version: '1.0.0',
      },
    },
  }),
  'test-dep-a-vuln': {
    'package.json': JSON.stringify({
      name: 'test-dep-a',
      version: '1.0.0',
    }),
    'vulnerable.txt': 'vulnerable test-dep-a',
  },
  'test-dep-a-fixed': {
    'package.json': JSON.stringify({
      name: 'test-dep-a',
      version: '1.0.1',
    }),
    'fixed.txt': 'fixed test-dep-a',
  },
}

t.test('normal audit', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: tree,
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })

  const manifest = registry.manifest({
    name: 'test-dep-a',
    packuments: [{ version: '1.0.0' }, { version: '1.0.1' }],
  })
  await registry.package({ manifest })
  const advisory = registry.advisory({
    id: 100,
    vulnerable_versions: '<1.0.1',
  })
  const bulkBody = gzip(JSON.stringify({ 'test-dep-a': ['1.0.0'] }))
  registry.nock.post('/-/npm/v1/security/advisories/bulk', bulkBody)
    .reply(200, {
      'test-dep-a': [advisory],
    })

  await npm.exec('audit', [])
  t.ok(process.exitCode, 'would have exited uncleanly')
  process.exitCode = 0
  t.matchSnapshot(joinedOutput())
})

t.test('fallback audit ', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: tree,
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  const manifest = registry.manifest({
    name: 'test-dep-a',
    packuments: [{ version: '1.0.0' }, { version: '1.0.1' }],
  })
  await registry.package({ manifest })
  const advisory = registry.advisory({
    id: 100,
    module_name: 'test-dep-a',
    vulnerable_versions: '<1.0.1',
    findings: [{ version: '1.0.0', paths: ['test-dep-a'] }],
  })
  registry.nock
    .post('/-/npm/v1/security/advisories/bulk').reply(404)
    .post('/-/npm/v1/security/audits/quick', body => {
      const unzipped = JSON.parse(gunzip(Buffer.from(body, 'hex')))
      return t.match(unzipped, {
        name: 'test-dep',
        version: '1.0.0',
        requires: { 'test-dep-a': '*' },
        dependencies: { 'test-dep-a': { version: '1.0.0' } },
      })
    }).reply(200, {
      actions: [],
      muted: [],
      advisories: {
        100: advisory,
      },
      metadata: {
        vulnerabilities: { info: 0, low: 0, moderate: 0, high: 1, critical: 0 },
        dependencies: 1,
        devDependencies: 0,
        optionalDependencies: 0,
        totalDependencies: 1,
      },
    })
  await npm.exec('audit', [])
  t.ok(process.exitCode, 'would have exited uncleanly')
  process.exitCode = 0
  t.matchSnapshot(joinedOutput())
})

t.test('json audit', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: tree,
    config: {
      json: true,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })

  const manifest = registry.manifest({
    name: 'test-dep-a',
    packuments: [{ version: '1.0.0' }, { version: '1.0.1' }],
  })
  await registry.package({ manifest })
  const advisory = registry.advisory({ id: 100 })
  const bulkBody = gzip(JSON.stringify({ 'test-dep-a': ['1.0.0'] }))
  registry.nock.post('/-/npm/v1/security/advisories/bulk', bulkBody)
    .reply(200, {
      'test-dep-a': [advisory],
    })

  await npm.exec('audit', [])
  t.ok(process.exitCode, 'would have exited uncleanly')
  process.exitCode = 0
  t.matchSnapshot(joinedOutput())
})

t.test('audit fix - bulk endpoint', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    prefixDir: tree,
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  const manifest = registry.manifest({
    name: 'test-dep-a',
    packuments: [{ version: '1.0.0' }, { version: '1.0.1' }],
  })
  await registry.package({
    manifest,
    tarballs: {
      '1.0.1': path.join(npm.prefix, 'test-dep-a-fixed'),
    },
  })
  const advisory = registry.advisory({ id: 100, vulnerable_versions: '1.0.0' })
  registry.nock.post('/-/npm/v1/security/advisories/bulk', body => {
    const unzipped = JSON.parse(gunzip(Buffer.from(body, 'hex')))
    return t.same(unzipped, { 'test-dep-a': ['1.0.0'] })
  })
    .reply(200, { // first audit
      'test-dep-a': [advisory],
    })
    .post('/-/npm/v1/security/advisories/bulk', body => {
      const unzipped = JSON.parse(gunzip(Buffer.from(body, 'hex')))
      return t.same(unzipped, { 'test-dep-a': ['1.0.1'] })
    })
    .reply(200, { // after fix
      'test-dep-a': [],
    })
  await npm.exec('audit', ['fix'])
  t.matchSnapshot(joinedOutput())
  const pkg = fs.readFileSync(path.join(npm.prefix, 'package-lock.json'), 'utf8')
  t.matchSnapshot(pkg, 'lockfile has test-dep-a@1.0.1')
  t.ok(
    fs.existsSync(path.join(npm.prefix, 'node_modules', 'test-dep-a', 'fixed.txt')),
    'has test-dep-a@1.0.1 on disk'
  )
})

t.test('completion', async t => {
  const { npm } = await loadMockNpm(t)
  const audit = await npm.cmd('audit')
  t.test('fix', async t => {
    await t.resolveMatch(
      audit.completion({ conf: { argv: { remain: ['npm', 'audit'] } } }),
      ['fix'],
      'completes to fix'
    )
  })

  t.test('subcommand fix', async t => {
    await t.resolveMatch(
      audit.completion({ conf: { argv: { remain: ['npm', 'audit', 'fix'] } } }),
      [],
      'resolves to ?'
    )
  })

  t.test('subcommand not recognized', async t => {
    await t.rejects(audit.completion({ conf: { argv: { remain: ['npm', 'audit', 'repare'] } } }), {
      message: 'repare not recognized',
    })
  })
})
