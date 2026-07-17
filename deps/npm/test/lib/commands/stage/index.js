const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm')
const MockRegistry = require('@npmcli/mock-registry')
const path = require('node:path')
const fs = require('node:fs')

const pkg = '@npmcli/test-package'
const token = 'test-auth-token'
const authConfig = { '//registry.npmjs.org/:_authToken': token }

const pkgJson = {
  name: pkg,
  description: 'npm test package',
  version: '1.0.0',
}

t.test('stages a package from cwd', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.match(joinedOutput(), /\+ @npmcli\/test-package@1\.0\.0 \(staged\)/)
})

t.test('stages with --dry-run', async t => {
  const { npm, joinedOutput, logs } = await loadMockNpm(t, {
    config: { ...authConfig, 'dry-run': true },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  await npm.exec('stage', ['publish'])
  t.ok(logs.notice.some(n => /Staging to .* \(dry-run\)/.test(n)))
  t.match(joinedOutput(), /\+ @npmcli\/test-package@1\.0\.0 \(staged\)/)
})

t.test('stages with --json', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig, json: true },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  const out = JSON.parse(joinedOutput())
  t.equal(out[pkg].name, pkg)
  t.equal(out[pkg].version, '1.0.0')
})

t.test('stages with --json includes stageId', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig, json: true },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  const stageId = 'f8e7a45b-7a5f-4f31-8e6d-9dd1c6ef38c0'
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, { stageId })
  await npm.exec('stage', ['publish'])
  const out = JSON.parse(joinedOutput())
  t.equal(out[pkg].name, pkg)
  t.equal(out[pkg].stageId, stageId)
})

t.test('completion returns subcommands', async t => {
  const Stage = require('../../../../lib/commands/stage/index.js')
  const res = await Stage.completion({ conf: { argv: { remain: ['npm', 'stage'] } } })
  t.strictSame(res, ['publish', 'list', 'view', 'approve', 'reject', 'download'])
})

t.test('completion returns empty for subcommand', async t => {
  const Stage = require('../../../../lib/commands/stage/index.js')
  const res = await Stage.completion({ conf: { argv: { remain: ['npm', 'stage', 'publish'] } } })
  t.strictSame(res, [])
})

t.test('throws on invalid semver tag', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { ...authConfig, tag: '1.0.0' },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  await t.rejects(npm.exec('stage', ['publish']), {
    message: /Tag name must not be a valid SemVer range/,
  })
})

t.test('throws ENEEDAUTH with no credentials', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {},
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  await t.rejects(npm.exec('stage', ['publish']), {
    code: 'ENEEDAUTH',
  })
})

t.test('warns on --dry-run with no credentials', async t => {
  const { npm, logs } = await loadMockNpm(t, {
    config: { 'dry-run': true },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  await npm.exec('stage', ['publish'])
  t.match(logs.warn, [/requires you to be logged in.*\(dry-run\)/])
})

t.test('stages a package with positional arg', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish', '.'])
  t.match(joinedOutput(), /\+ @npmcli\/test-package@1\.0\.0 \(staged\)/)
})

t.test('respects ignore-scripts', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig, 'ignore-scripts': true },
    prefixDir: {
      'package.json': JSON.stringify({
        ...pkgJson,
        scripts: {
          prepublishOnly: 'exit 1',
          publish: 'exit 1',
          postpublish: 'exit 1',
        },
      }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.match(joinedOutput(), /\(staged\)/)
})

t.test('foreground-scripts can be set to false', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig, 'foreground-scripts': false },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.match(joinedOutput(), /\(staged\)/)
})

t.test('runs lifecycle scripts', async t => {
  const { npm, prefix } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {
      'package.json': JSON.stringify({
        ...pkgJson,
        scripts: {
          prepublishOnly: 'touch scripts-prepublishonly',
          publish: 'touch scripts-publish',
          postpublish: 'touch scripts-postpublish',
        },
      }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.equal(fs.existsSync(path.join(prefix, 'scripts-prepublishonly')), true)
  t.equal(fs.existsSync(path.join(prefix, 'scripts-publish')), true)
  t.equal(fs.existsSync(path.join(prefix, 'scripts-postpublish')), true)
})

t.test('respects publishConfig', async t => {
  const alternateRegistry = 'https://other.registry.npmjs.org'
  const { npm, joinedOutput, logs } = await loadMockNpm(t, {
    config: {
      [`${alternateRegistry.slice(6)}/:_authToken`]: 'test-other-token',
    },
    prefixDir: {
      'package.json': JSON.stringify({
        ...pkgJson,
        publishConfig: {
          registry: alternateRegistry,
          other: 'unknown-key',
        },
      }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: alternateRegistry,
    authorization: 'test-other-token',
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.match(joinedOutput(), /\(staged\)/)
  t.match(logs.warn, [/Unknown publishConfig/])
})

t.test('warns about auto-corrected package.json errors', async t => {
  const { npm, logs } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {
      'package.json': JSON.stringify({
        name: pkg,
        version: '1.0.0',
        repository: 'github:user/repo',
      }),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.ok(logs.warn.some(w => /auto-corrected/.test(w)))
  t.ok(logs.warn.some(w => /errors corrected/.test(w)))
})

t.test('stages with basic auth (username)', async t => {
  const basic = Buffer.from('test-user:test-password').toString('base64')
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:username': 'test-user',
      '//registry.npmjs.org/:_password': Buffer.from('test-password').toString('base64'),
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    basic,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.match(joinedOutput(), /\(staged\)/)
})

t.test('stages with cert auth', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:certfile': '/path/to/cert',
      '//registry.npmjs.org/:keyfile': '/path/to/key',
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, {})
  await npm.exec('stage', ['publish'])
  t.match(joinedOutput(), /\(staged\)/)
})

t.test('throws EPRIVATE for private packages', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {
      'package.json': JSON.stringify({ ...pkgJson, private: true }),
    },
  })
  await t.rejects(npm.exec('stage', ['publish']), {
    code: 'EPRIVATE',
  })
})

t.test('outputs stageId when returned', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.post('/-/stage/package/@npmcli%2ftest-package').reply(201, { stageId: 'abc-123' })
  await npm.exec('stage', ['publish'])
  t.match(joinedOutput(), /staged with id abc-123/)
})
