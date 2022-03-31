const t = require('tap')
const path = require('path')
const tspawk = require('../../fixtures/tspawk')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

const spawk = tspawk(t)

// TODO this ... smells.  npm "script-shell" config mentions defaults but those
// are handled by run-script, not npm.  So for now we have to tie tests to some
// pretty specific internals of runScript
const makeSpawnArgs = require('@npmcli/run-script/lib/make-spawn-args.js')

const npmConfig = {
  config: {
    'ignore-scripts': false,
    editor: 'testeditor',
  },
  prefixDir: {
    node_modules: {
      semver: {
        'package.json': JSON.stringify({
          scripts: {
            install: 'testinstall',
          },
        }),
        node_modules: {
          abbrev: {},
        },
      },
      '@npmcli': {
        'scoped-package': {},
      },
    },
  },
}

t.test('npm edit', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, npmConfig)

  const semverPath = path.resolve(npm.prefix, 'node_modules', 'semver')
  const [scriptShell] = makeSpawnArgs({
    event: 'install',
    path: npm.prefix,
  })
  spawk.spawn('testeditor', [semverPath])
  spawk.spawn(
    scriptShell,
    args => args.includes('testinstall'),
    { cwd: semverPath }
  )
  await npm.exec('edit', ['semver'])
  t.match(joinedOutput(), 'rebuilt dependencies successfully')
})

t.test('rebuild failure', async t => {
  const { npm } = await loadMockNpm(t, npmConfig)
  const semverPath = path.resolve(npm.prefix, 'node_modules', 'semver')
  const [scriptShell] = makeSpawnArgs({
    event: 'install',
    path: npm.prefix,
  })
  spawk.spawn('testeditor', [semverPath])
  spawk.spawn(
    scriptShell,
    args => args.includes('testinstall'),
    { cwd: semverPath }
  ).exit(1).stdout('test error')
  await t.rejects(
    npm.exec('edit', ['semver']),
    { message: 'command failed' }
  )
})

t.test('editor failure', async t => {
  const { npm } = await loadMockNpm(t, npmConfig)
  const semverPath = path.resolve(npm.prefix, 'node_modules', 'semver')
  spawk.spawn('testeditor', [semverPath]).exit(1).stdout('test editor failure')
  await t.rejects(
    npm.exec('edit', ['semver']),
    { message: 'editor process exited with code: 1' }
  )
})

t.test('npm edit editor has flags', async t => {
  const { npm } = await loadMockNpm(t, {
    ...npmConfig,
    config: {
      ...npmConfig.config,
      editor: 'testeditor --flag',
    },
  })

  const semverPath = path.resolve(npm.prefix, 'node_modules', 'semver')
  const [scriptShell] = makeSpawnArgs({
    event: 'install',
    path: npm.prefix,
  })
  spawk.spawn('testeditor', ['--flag', semverPath])
  spawk.spawn(
    scriptShell,
    args => args.includes('testinstall'),
    { cwd: semverPath }
  )
  await npm.exec('edit', ['semver'])
})

t.test('npm edit no args', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('edit', []),
    { code: 'EUSAGE' },
    'throws usage error'
  )
})

t.test('npm edit nonexistent package', async t => {
  const { npm } = await loadMockNpm(t, npmConfig)

  await t.rejects(
    npm.exec('edit', ['abbrev']),
    /lstat/
  )
})

t.test('scoped package', async t => {
  const { npm } = await loadMockNpm(t, npmConfig)
  const scopedPath = path.resolve(npm.prefix, 'node_modules', '@npmcli', 'scoped-package')
  spawk.spawn('testeditor', [scopedPath])
  await npm.exec('edit', ['@npmcli/scoped-package'])
})

t.test('subdependency', async t => {
  const { npm } = await loadMockNpm(t, npmConfig)
  const subdepPath = path.resolve(npm.prefix, 'node_modules', 'semver', 'node_modules', 'abbrev')
  spawk.spawn('testeditor', [subdepPath])
  await npm.exec('edit', ['semver/abbrev'])
})
