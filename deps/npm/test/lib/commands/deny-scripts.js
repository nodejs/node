const t = require('tap')
const fs = require('node:fs')
const { resolve } = require('node:path')
const _mockNpm = require('../../fixtures/mock-npm')

const setupProject = ({ allowScripts, withScripts = ['core-js'] } = {}) => {
  const pkg = {
    name: 'host',
    version: '1.0.0',
    dependencies: Object.fromEntries(withScripts.map((n) => [n, '*'])),
  }
  if (allowScripts !== undefined) {
    pkg.allowScripts = allowScripts
  }
  const lockPackages = { '': pkg }
  const nodeModules = {}
  for (const name of withScripts) {
    nodeModules[name] = {
      'package.json': JSON.stringify({
        name,
        version: '1.0.0',
        scripts: { install: 'echo install' },
      }),
    }
    lockPackages[`node_modules/${name}`] = {
      version: '1.0.0',
      resolved: `https://registry.npmjs.org/${name}/-/${name}-1.0.0.tgz`,
      hasInstallScript: true,
    }
  }
  return {
    'package.json': JSON.stringify(pkg, null, 2),
    'package-lock.json': JSON.stringify({
      name: pkg.name,
      version: pkg.version,
      lockfileVersion: 3,
      requires: true,
      packages: lockPackages,
    }),
    node_modules: nodeModules,
  }
}

t.test('deny-scripts <pkg> writes name-only false entry', async t => {
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['core-js'] }),
  })
  await npm.exec('deny-scripts', ['core-js'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'core-js': false })
})

t.test('deny-scripts <pkg> ignores --pin and always writes name-only', async t => {
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['core-js'] }),
    config: { 'allow-scripts-pin': true },
  })
  await npm.exec('deny-scripts', ['core-js'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'core-js': false })
})

t.test('deny-scripts <pkg> replaces existing pinned allow', async t => {
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['core-js'],
      allowScripts: { 'core-js@1.0.0': true },
    }),
  })
  await npm.exec('deny-scripts', ['core-js'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'core-js': false })
})

t.test('deny-scripts --pending is rejected', async t => {
  const { npm } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['core-js'] }),
    config: { 'allow-scripts-pending': true },
  })
  await t.rejects(npm.exec('deny-scripts', []), {
    code: 'EUSAGE',
    message: /`npm deny-scripts --allow-scripts-pending` is not supported; run `npm install-scripts ls` to list unreviewed packages/,
  })
})

t.test('deny-scripts --all denies every unreviewed package', async t => {
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['core-js', 'telemetry'] }),
    config: { all: true },
  })
  await npm.exec('deny-scripts', [])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'core-js': false, telemetry: false })
})

t.test('deny-scripts errors on unknown package', async t => {
  const { npm } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['core-js'] }),
  })
  await t.rejects(
    npm.exec('deny-scripts', ['not-installed']),
    { code: 'ENOMATCH' }
  )
})

t.test('deny-scripts <pkg@version> denies a dotted name with a version specifier', async t => {
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['cordova.plugins.diagnostic'] }),
  })
  await npm.exec('deny-scripts', ['cordova.plugins.diagnostic@1.0.0'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'cordova.plugins.diagnostic': false })
})

t.test('deny-scripts requires positional args or --all', async t => {
  const { npm } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['core-js'] }),
  })
  await t.rejects(npm.exec('deny-scripts', []), { code: 'EUSAGE' })
})

t.test('deny-scripts --all with no unreviewed packages prints message', async t => {
  const { npm, joinedOutput } = await _mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'host', version: '1.0.0' }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: { '': { name: 'host', version: '1.0.0' } },
      }),
      node_modules: {},
    },
    config: { all: true },
  })
  await npm.exec('deny-scripts', [])
  t.match(joinedOutput(), /No packages with unreviewed install scripts/)
})

t.test('deny-scripts --all --json with no unreviewed emits empty list', async t => {
  const { npm, joinedOutput } = await _mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'host', version: '1.0.0' }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: { '': { name: 'host', version: '1.0.0' } },
      }),
      node_modules: {},
    },
    config: { all: true, json: true },
  })
  await npm.exec('deny-scripts', [])
  t.strictSame(JSON.parse(joinedOutput()), { allowScripts: [] })
})

t.test('deny-scripts fails on global', async t => {
  const { npm } = await _mockNpm(t, {
    config: { global: true },
  })
  await t.rejects(npm.exec('deny-scripts', ['canvas']), { code: 'EGLOBAL' })
})

t.test('deny-scripts <pkg> on a package already denied is no-op', async t => {
  const { npm, joinedOutput, prefix } = await _mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['core-js'],
      allowScripts: { 'core-js': false },
    }),
  })
  await npm.exec('deny-scripts', ['core-js'])
  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'core-js': false })
  t.match(joinedOutput(), /Nothing to deny/)
})

t.test('deny-scripts --json outputs structured summary', async t => {
  const { npm, joinedOutput } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['core-js'] }),
    config: { json: true },
  })
  await npm.exec('deny-scripts', ['core-js'])
  const parsed = JSON.parse(joinedOutput())
  t.match(parsed, {
    allowScripts: [{ name: 'core-js', changes: [{ key: 'core-js', change: 'added' }] }],
  })
})
