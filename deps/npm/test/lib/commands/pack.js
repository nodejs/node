const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const path = require('path')
const fs = require('fs')

t.test('should pack current directory with no arguments', async t => {
  const { npm, outputs, logs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'test-package',
        version: '1.0.0',
      }),
    },
  })
  await npm.exec('pack', [])
  const filename = 'test-package-1.0.0.tgz'
  t.strictSame(outputs, [[filename]])
  t.matchSnapshot(logs.notice.map(([, m]) => m), 'logs pack contents')
  t.ok(fs.statSync(path.resolve(npm.prefix, filename)))
})

t.test('follows pack-destination config', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'test-package',
        version: '1.0.0',
      }),
      'tar-destination': {},
    },
    config: ({ prefix }) => ({ 'pack-destination': path.join(prefix, 'tar-destination') }),
  })
  await npm.exec('pack', [])
  const filename = 'test-package-1.0.0.tgz'
  t.strictSame(outputs, [[filename]])
  t.ok(fs.statSync(path.resolve(npm.prefix, 'tar-destination', filename)))
})

t.test('should pack given directory for scoped package', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npm/test-package',
        version: '1.0.0',
      }),
    },
  })
  await npm.exec('pack', [])
  const filename = 'npm-test-package-1.0.0.tgz'
  t.strictSame(outputs, [[filename]])
  t.ok(fs.statSync(path.resolve(npm.prefix, filename)))
})

t.test('should log output as valid json', async t => {
  const { npm, outputs, logs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'test-package',
        version: '1.0.0',
      }),
    },
    config: { json: true },
  })
  await npm.exec('pack', [])
  const filename = 'test-package-1.0.0.tgz'
  t.matchSnapshot(outputs.map(JSON.parse), 'outputs as json')
  t.matchSnapshot(logs.notice.map(([, m]) => m), 'logs pack contents')
  t.ok(fs.statSync(path.resolve(npm.prefix, filename)))
})

t.test('should log scoped package output as valid json', async t => {
  const { npm, outputs, logs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@myscope/test-package',
        version: '1.0.0',
      }),
    },
    config: { json: true },
  })
  await npm.exec('pack', [])
  const filename = 'myscope-test-package-1.0.0.tgz'
  t.matchSnapshot(outputs.map(JSON.parse), 'outputs as json')
  t.matchSnapshot(logs.notice.map(([, m]) => m), 'logs pack contents')
  t.ok(fs.statSync(path.resolve(npm.prefix, filename)))
})

t.test('dry run', async t => {
  const { npm, outputs, logs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'test-package',
        version: '1.0.0',
      }),
    },
    config: { 'dry-run': true },
  })
  await npm.exec('pack', [])
  const filename = 'test-package-1.0.0.tgz'
  t.strictSame(outputs, [[filename]])
  t.matchSnapshot(logs.notice.map(([, m]) => m), 'logs pack contents')
  t.throws(() => fs.statSync(path.resolve(npm.prefix, filename)))
})

t.test('invalid packument', async t => {
  const { npm, outputs } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': '{}',
    },
  })
  await t.rejects(
    npm.exec('pack', []),
    /Invalid package, must have name and version/
  )
  t.strictSame(outputs, [])
})

t.test('workspaces', async t => {
  const loadWorkspaces = (t) => loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify(
        {
          name: 'workspaces-test',
          version: '1.0.0',
          workspaces: ['workspace-a', 'workspace-b'],
        },
        null,
        2
      ),
      'workspace-a': {
        'package.json': JSON.stringify({
          name: 'workspace-a',
          version: '1.0.0',
        }),
      },
      'workspace-b': {
        'package.json': JSON.stringify({
          name: 'workspace-b',
          version: '1.0.0',
        }),
      },
    },
    config: {
      workspaces: true,
      // TODO: this is a workaround for npm run test-all
      // somehow leaking include-workspace-root
      'include-workspace-root': false,
    },
  })

  t.test('all workspaces', async t => {
    const { npm, outputs } = await loadWorkspaces(t)
    await npm.exec('pack', [])
    t.strictSame(outputs, [['workspace-a-1.0.0.tgz'], ['workspace-b-1.0.0.tgz']])
  })

  t.test('all workspaces, `.` first arg', async t => {
    const { npm, outputs } = await loadWorkspaces(t)
    await npm.exec('pack', ['.'])
    t.strictSame(outputs, [['workspace-a-1.0.0.tgz'], ['workspace-b-1.0.0.tgz']])
  })

  t.test('one workspace', async t => {
    const { npm, outputs } = await loadWorkspaces(t)
    await npm.exec('pack', ['workspace-a'])
    t.strictSame(outputs, [['workspace-a-1.0.0.tgz']])
  })

  t.test('specific package', async t => {
    const { npm, outputs } = await loadWorkspaces(t)
    await npm.exec('pack', [npm.prefix])
    t.strictSame(outputs, [['workspaces-test-1.0.0.tgz']])
  })
})
