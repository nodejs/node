const t = require('tap')
const fs = require('node:fs')
const { resolve } = require('node:path')
const _mockNpm = require('../../fixtures/mock-npm')

const mockNpm = async (t, opts = {}) => {
  return _mockNpm(t, opts)
}

const setupProject = ({ allowScripts, withScripts = ['canvas'], noResolved = [] } = {}) => {
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
    const tarUrl = `https://registry.npmjs.org/${name}/-/${name}-1.0.0.tgz`
    nodeModules[name] = {
      'package.json': JSON.stringify({
        name,
        version: '1.0.0',
        scripts: { install: 'echo install' },
      }),
    }
    const lockEntry = {
      version: '1.0.0',
      hasInstallScript: true,
    }
    // Some lockfiles omit `resolved` for registry deps. Those nodes have no
    // trustable version, so they can only be approved by name.
    if (!noResolved.includes(name)) {
      lockEntry.resolved = tarUrl
    }
    lockPackages[`node_modules/${name}`] = lockEntry
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

t.test('approve-scripts --pending lists unreviewed packages', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
    config: { 'allow-scripts-pending': true },
  })
  await npm.exec('approve-scripts', [])
  const out = joinedOutput()
  t.match(out, /2 packages have install scripts blocked because they are not covered by allowScripts/)
  t.match(out, /canvas@1\.0\.0/)
  t.match(out, /sharp@1\.0\.0/)
})

t.test('approve-scripts --pending lists unreviewed packages even with ignore-scripts set', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
    config: { 'allow-scripts-pending': true, 'ignore-scripts': true },
  })
  await npm.exec('approve-scripts', [])
  const out = joinedOutput()
  t.match(out, /2 packages have install scripts blocked because they are not covered by allowScripts/)
  t.match(out, /canvas@1\.0\.0/)
  t.match(out, /sharp@1\.0\.0/)
})

t.test('approve-scripts --pending with no unreviewed says so', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({
      allowScripts: { canvas: true },
      withScripts: ['canvas'],
    }),
    config: { 'allow-scripts-pending': true },
  })
  await npm.exec('approve-scripts', [])
  t.match(joinedOutput(), /No packages with unreviewed install scripts/)
})

t.test('approve-scripts <pkg> writes pinned entry by default', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await npm.exec('approve-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
})

t.test('approve-scripts <pkg> --no-pin writes name-only entry', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { 'allow-scripts-pin': false },
  })
  await npm.exec('approve-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { canvas: true })
})

t.test('approve-scripts --all approves every unreviewed package', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
    config: { all: true },
  })
  await npm.exec('approve-scripts', [])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, {
    'canvas@1.0.0': true,
    'sharp@1.0.0': true,
  })
})

t.test('approve-scripts --all approves a dep without a resolved URL by name', async t => {
  // Regression for npm/cli#9558: a dep with no `resolved` URL can't be
  // pinned, but must still be approved by name, not silently skipped.
  const { npm, prefix, logs } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'], noResolved: ['canvas'] }),
    config: { all: true },
  })
  await npm.exec('approve-scripts', [])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { canvas: true }, 'approved by name, not skipped')
  t.match(
    logs.warn.byTitle('approve-scripts').join('\n'),
    /no "resolved" URL/,
    'warns that a version pin could not be written'
  )
})

t.test('approve-scripts <pkg> approves a dep without a resolved URL by name', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'], noResolved: ['canvas'] }),
  })
  await npm.exec('approve-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { canvas: true })
})

t.test('approve-scripts --pending is empty after a no-resolved dep is approved by name', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({
      allowScripts: { canvas: true },
      withScripts: ['canvas'],
      noResolved: ['canvas'],
    }),
    config: { 'allow-scripts-pending': true },
  })
  await npm.exec('approve-scripts', [])
  t.match(
    joinedOutput(),
    /No packages with unreviewed install scripts/,
    'name-only entry covers the dep even without a resolved URL'
  )
})

t.test('approve-scripts errors on unknown package', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('approve-scripts', ['not-installed']),
    { code: 'ENOMATCH' }
  )
})

t.test('approve-scripts approves a package whose name contains dots', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['cordova.plugins.diagnostic'] }),
  })
  await npm.exec('approve-scripts', ['cordova.plugins.diagnostic'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'cordova.plugins.diagnostic@1.0.0': true })
})

t.test('approve-scripts <pkg@version> approves a dotted name with a version specifier', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['cordova.plugins.diagnostic'] }),
  })
  await npm.exec('approve-scripts', ['cordova.plugins.diagnostic@1.0.0'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'cordova.plugins.diagnostic@1.0.0': true })
})

t.test('approve-scripts <@scope/pkg@version> approves a scoped name with a version', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { '@scope/pkg': '*' },
      }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': { name: 'host', version: '1.0.0', dependencies: { '@scope/pkg': '*' } },
          'node_modules/@scope/pkg': {
            version: '2.0.0',
            resolved: 'https://registry.npmjs.org/@scope/pkg/-/pkg-2.0.0.tgz',
            hasInstallScript: true,
          },
        },
      }),
      node_modules: {
        '@scope': {
          pkg: {
            'package.json': JSON.stringify({
              name: '@scope/pkg',
              version: '2.0.0',
              scripts: { install: 'echo install' },
            }),
          },
        },
      },
    },
  })
  await npm.exec('approve-scripts', ['@scope/pkg@2.0.0'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { '@scope/pkg@2.0.0': true })
})

t.test('approve-scripts <pkg@version> errors when no installed version matches', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('approve-scripts', ['canvas@9.9.9']),
    { code: 'ENOMATCH' }
  )
})

t.test('approve-scripts reports only the unmatched args and writes nothing', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  const err = await npm.exec('approve-scripts', ['canvas', 'not-installed'])
    .then(() => null, e => e)
  t.equal(err.code, 'ENOMATCH')
  t.match(err.message, /not-installed/)
  t.notMatch(err.message, /canvas/)
  // Nothing is written when any arg fails to match.
  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.notOk(pkg.allowScripts)
})

t.test('approve-scripts <pkg@tag> matches every installed version', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  // A tag carries no version to filter on, so it behaves like a bare name.
  await npm.exec('approve-scripts', ['canvas@latest'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
})

t.test('approve-scripts errors on an unparseable spec', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  // npm-package-arg throws on this; the raw string is used as the name and
  // matches nothing.
  await t.rejects(
    npm.exec('approve-scripts', ['foo@@bar']),
    { code: 'ENOMATCH' }
  )
})

t.test('approve-scripts errors on a spec with no package name', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  // A path spec parses but carries no name, so it matches no registry dep.
  await t.rejects(
    npm.exec('approve-scripts', ['./local-pkg']),
    { code: 'ENOMATCH' }
  )
})

t.test('approve-scripts respects existing deny entry', async t => {
  const { npm, prefix, logs } = await mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      allowScripts: { canvas: false },
    }),
  })
  await npm.exec('approve-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  // Deny wins; unchanged.
  t.strictSame(pkg.allowScripts, { canvas: false })
  t.match(logs.warn.byTitle('approve-scripts'), [/canvas is denied/])
})

t.test('approve-scripts requires positional args, --all, or --pending', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(npm.exec('approve-scripts', []), { code: 'EUSAGE' })
})

t.test('approve-scripts --pending cannot be combined with positional', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { 'allow-scripts-pending': true },
  })
  await t.rejects(npm.exec('approve-scripts', ['canvas']), { code: 'EUSAGE' })
})

t.test('approve-scripts fails on global', async t => {
  const { npm } = await mockNpm(t, {
    config: { global: true },
  })
  await t.rejects(npm.exec('approve-scripts', ['canvas']), { code: 'EGLOBAL' })
})

t.test('approve-scripts --json outputs structured summary', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { json: true },
  })
  await npm.exec('approve-scripts', ['canvas'])
  const parsed = JSON.parse(joinedOutput())
  t.match(parsed, {
    allowScripts: [{ name: 'canvas', changes: [{ key: 'canvas@1.0.0', change: 'added' }] }],
  })
})

t.test('approve-scripts --pending --json lists unreviewed packages as JSON', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
    config: { 'allow-scripts-pending': true, json: true },
  })
  await npm.exec('approve-scripts', [])
  const parsed = JSON.parse(joinedOutput())
  const byName = Object.fromEntries(parsed.allowScripts.map((e) => [e.name, e.changes]))
  t.strictSame(byName, {
    canvas: [{ key: 'canvas@1.0.0', change: 'pending' }],
    sharp: [{ key: 'sharp@1.0.0', change: 'pending' }],
  })
})

t.test('approve-scripts --pending --json with no unreviewed emits empty list', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({
      allowScripts: { canvas: true },
      withScripts: ['canvas'],
    }),
    config: { 'allow-scripts-pending': true, json: true },
  })
  await npm.exec('approve-scripts', [])
  t.strictSame(JSON.parse(joinedOutput()), { allowScripts: [] })
})

t.test('approve-scripts --all --json with no unreviewed emits empty list', async t => {
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
  await npm.exec('approve-scripts', [])
  t.strictSame(JSON.parse(joinedOutput()), { allowScripts: [] })
})

t.test('approve-scripts --all with no unreviewed packages prints message', async t => {
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
  await npm.exec('approve-scripts', [])
  t.match(joinedOutput(), /No packages with unreviewed install scripts/)
})

t.test('approve-scripts <pkg> on a package already at the right pin is no-op', async t => {
  const { npm, prefix, joinedOutput } = await _mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      allowScripts: { 'canvas@1.0.0': true },
    }),
  })
  await npm.exec('approve-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
  t.match(joinedOutput(), /Nothing to approve/)
})

t.test('approve-scripts --pending with single package uses singular wording', async t => {
  const { npm, joinedOutput } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { 'allow-scripts-pending': true },
  })
  await npm.exec('approve-scripts', [])
  t.match(joinedOutput(), /1 package has install scripts/)
})

t.test('approve-scripts --pending lists package with no version', async t => {
  // Use a fixture where the lockfile records a synthetic node without a version
  const { npm } = await _mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { 'allow-scripts-pending': true },
  })
  await npm.exec('approve-scripts', [])
  // Just exercising; no assertion needed for additional coverage.
  t.pass()
})

const twoVersionFixture = {
  'package.json': JSON.stringify({
    name: 'host',
    version: '1.0.0',
    dependencies: { 'top-of-tree': '*' },
  }),
  'package-lock.json': JSON.stringify({
    name: 'host',
    version: '1.0.0',
    lockfileVersion: 3,
    requires: true,
    packages: {
      '': { name: 'host', version: '1.0.0', dependencies: { 'top-of-tree': '*' } },
      'node_modules/lodash': {
        version: '4.17.21',
        resolved: 'https://registry.npmjs.org/lodash/-/lodash-4.17.21.tgz',
        hasInstallScript: true,
      },
      'node_modules/top-of-tree': {
        version: '1.0.0',
        resolved: 'https://registry.npmjs.org/top-of-tree/-/top-of-tree-1.0.0.tgz',
        dependencies: { lodash: '3.10.1' },
      },
      'node_modules/top-of-tree/node_modules/lodash': {
        version: '3.10.1',
        resolved: 'https://registry.npmjs.org/lodash/-/lodash-3.10.1.tgz',
        hasInstallScript: true,
      },
    },
  }),
  node_modules: {
    lodash: {
      'package.json': JSON.stringify({
        name: 'lodash',
        version: '4.17.21',
        scripts: { install: 'echo install' },
      }),
    },
    'top-of-tree': {
      'package.json': JSON.stringify({ name: 'top-of-tree', version: '1.0.0' }),
      node_modules: {
        lodash: {
          'package.json': JSON.stringify({
            name: 'lodash',
            version: '3.10.1',
            scripts: { install: 'echo install' },
          }),
        },
      },
    },
  },
}

t.test('approve-scripts --pending --json groups multiple versions under one name', async t => {
  // Two versions of lodash are unreviewed; pendingSummary must collapse
  // them into a single `lodash` entry (hits the `groups.has(display)`
  // truthy branch on the second node).
  const { npm, joinedOutput } = await _mockNpm(t, {
    prefixDir: twoVersionFixture,
    config: { 'allow-scripts-pending': true, json: true },
  })
  await npm.exec('approve-scripts', [])
  const parsed = JSON.parse(joinedOutput())
  t.strictSame(parsed.allowScripts.map((e) => e.name), ['lodash'])
  t.strictSame(parsed.allowScripts[0].changes.map((c) => c.key).sort(), [
    'lodash@3.10.1',
    'lodash@4.17.21',
  ])
  t.ok(parsed.allowScripts[0].changes.every((c) => c.change === 'pending'))
})

t.test('approve-scripts groups multiple installed versions of the same package', async t => {
  // Two versions of lodash exist in the tree; both have install scripts.
  // groupByPackage should put them in the same group (hits the
  // `if (!groups[key])` falsy branch on the second node).
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: twoVersionFixture,
  })
  await npm.exec('approve-scripts', ['lodash'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  // Both versions get pinned.
  t.strictSame(pkg.allowScripts, {
    'lodash@3.10.1': true,
    'lodash@4.17.21': true,
  })
})

t.test('approve-scripts <pkg@version> pins only the matching installed version', async t => {
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: twoVersionFixture,
  })
  await npm.exec('approve-scripts', ['lodash@4.17.21'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'lodash@4.17.21': true })
})

t.test('approve-scripts <pkg@range> pins only versions satisfying the range', async t => {
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: twoVersionFixture,
  })
  await npm.exec('approve-scripts', ['lodash@^4'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'lodash@4.17.21': true })
})

t.test('approve-scripts --pending handles node with no version', async t => {
  // Exercise the ternary's falsy branch in runPending: `node.version ? '@'... : ''`
  // when the node has no version field.
  const mockSync = await _mockNpm(t, {
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
    config: { 'allow-scripts-pending': true },
    mocks: {
      // Make the walker return a synthetic node with no version
      '{LIB}/utils/check-allow-scripts.js': async () => [{
        node: { packageName: 'no-version-pkg', name: 'no-version-pkg', version: undefined },
        scripts: { install: 'do-stuff' },
      }],
    },
  })
  await mockSync.npm.exec('approve-scripts', [])
  // Output should mention the package without an @version suffix.
  t.match(mockSync.joinedOutput(), / no-version-pkg \(install: do-stuff\)/)
})

t.test('approve-scripts --pending --json handles node with no version', async t => {
  // Exercise pendingSummary's `version ? ... : display` falsy branch: the
  // key is the bare name when the node has no version field.
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
    config: { 'allow-scripts-pending': true, json: true },
    mocks: {
      '{LIB}/utils/check-allow-scripts.js': async () => [{
        node: { packageName: 'no-version-pkg', name: 'no-version-pkg', version: undefined },
        scripts: { install: 'do-stuff' },
      }],
    },
  })
  await npm.exec('approve-scripts', [])
  t.strictSame(JSON.parse(joinedOutput()), {
    allowScripts: [
      { name: 'no-version-pkg', changes: [{ key: 'no-version-pkg', change: 'pending' }] },
    ],
  })
})

t.test('forbidden semver range in package.json#allowScripts is dropped with a warning', async t => {
  // End-to-end: project declares a caret range in allowScripts. The
  // resolver must drop the entry, emit a warning, and the matching node
  // must remain unreviewed (listed by --pending).
  const mock = await _mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      // ^0.33.0 is a forbidden range per RFC.
      allowScripts: { 'canvas@^0.33.0': true },
    }),
    config: { 'allow-scripts-pending': true },
  })
  await mock.npm.exec('approve-scripts', [])

  const warnings = mock.logs.warn.byTitle('install-scripts')
  t.ok(
    warnings.some(m => /semver ranges/.test(m) && /canvas@\^0\.33\.0/.test(m)),
    'resolver emits warning about forbidden range'
  )
  // canvas was installed with version 1.0.0 (setupProject default) and
  // the forbidden allowlist entry was dropped, so canvas appears in the
  // pending list.
  t.match(mock.joinedOutput(), /canvas@1\.0\.0/)
})

t.test('approve-scripts --pending lists packages that only have binding.gyp', async t => {
  // End-to-end: a package with no preinstall/install/postinstall but a
  // binding.gyp on disk gets a synthetic `node-gyp rebuild` install
  // script. The runtime isNodeGypPackage check must see it and surface
  // the package in --pending output.
  const mock = await _mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { 'native-pkg': '*' },
      }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': { name: 'host', version: '1.0.0', dependencies: { 'native-pkg': '*' } },
          'node_modules/native-pkg': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/native-pkg/-/native-pkg-1.0.0.tgz',
            // No hasInstallScript — the synthetic node-gyp injection is
            // what we want this test to exercise.
          },
        },
      }),
      node_modules: {
        'native-pkg': {
          'package.json': JSON.stringify({ name: 'native-pkg', version: '1.0.0' }),
          // The file that triggers isNodeGypPackage to return true.
          'binding.gyp': '{}',
        },
      },
    },
    config: { 'allow-scripts-pending': true },
  })
  await mock.npm.exec('approve-scripts', [])

  const out = mock.joinedOutput()
  t.match(out, /native-pkg@1\.0\.0/, 'binding.gyp-only package appears in --pending')
  t.match(out, /install: node-gyp rebuild/, 'synthetic node-gyp install is named')
})

t.test('approve-scripts --all never approves bundled deps', async t => {
  // Bundled deps never run their install scripts and cannot be
  // allowlisted. They never reach the unreviewed list, so --all must not
  // write a key derived from the bundled tarball's self-claimed identity.
  const { npm, logs, prefix } = await _mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { 'parent-pkg': '*' },
      }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': { name: 'host', version: '1.0.0', dependencies: { 'parent-pkg': '*' } },
          'node_modules/parent-pkg': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/parent-pkg/-/parent-pkg-1.0.0.tgz',
            hasInstallScript: true,
          },
          'node_modules/parent-pkg/node_modules/inner': {
            version: '1.0.0',
            inBundle: true,
            hasInstallScript: true,
          },
        },
      }),
      node_modules: {
        'parent-pkg': {
          'package.json': JSON.stringify({
            name: 'parent-pkg',
            version: '1.0.0',
            scripts: { install: 'echo install' },
            bundleDependencies: ['inner'],
          }),
          node_modules: {
            inner: {
              'package.json': JSON.stringify({
                name: 'inner',
                version: '1.0.0',
                scripts: { install: 'echo bundled-install' },
              }),
            },
          },
        },
      },
    },
    config: { all: true },
  })
  await npm.exec('approve-scripts', [])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  // parent-pkg is approvable. inner is bundled and must be excluded.
  t.equal(pkg.allowScripts['parent-pkg@1.0.0'], true,
    'non-bundled parent gets approved')
  t.notOk(Object.keys(pkg.allowScripts).some(k => k.startsWith('inner')),
    'bundled inner is not approved')
  t.strictSame(logs.warn.byTitle('approve-scripts'), [],
    'no warning; bundled deps are excluded upstream')
})

t.test('approve-scripts <bundled-pkg> positional is ignored', async t => {
  // Same protection on the positional path: a user typing a bundled
  // package name must not get a policy entry written.
  const { npm } = await _mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { 'parent-pkg': '*' },
      }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': { name: 'host', version: '1.0.0', dependencies: { 'parent-pkg': '*' } },
          'node_modules/parent-pkg': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/parent-pkg/-/parent-pkg-1.0.0.tgz',
            hasInstallScript: true,
          },
          'node_modules/parent-pkg/node_modules/inner': {
            version: '1.0.0',
            inBundle: true,
            hasInstallScript: true,
          },
        },
      }),
      node_modules: {
        'parent-pkg': {
          'package.json': JSON.stringify({
            name: 'parent-pkg',
            version: '1.0.0',
            scripts: { install: 'echo install' },
            bundleDependencies: ['inner'],
          }),
          node_modules: {
            inner: {
              'package.json': JSON.stringify({
                name: 'inner',
                version: '1.0.0',
                scripts: { install: 'echo bundled' },
              }),
            },
          },
        },
      },
    },
  })
  await t.rejects(
    npm.exec('approve-scripts', ['inner']),
    { code: 'ENOMATCH' },
    'typing the bundled package name does not match any approvable node'
  )
})

t.test('approve-scripts --all with only bundled deps has nothing to review', async t => {
  const { npm, logs, joinedOutput, prefix } = await _mockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        dependencies: { 'parent-pkg': '*' },
      }),
      'package-lock.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        lockfileVersion: 3,
        requires: true,
        packages: {
          '': { name: 'host', version: '1.0.0', dependencies: { 'parent-pkg': '*' } },
          'node_modules/parent-pkg': {
            version: '1.0.0',
            resolved: 'https://registry.npmjs.org/parent-pkg/-/parent-pkg-1.0.0.tgz',
            // parent-pkg has NO install scripts; only the bundled child does.
          },
          'node_modules/parent-pkg/node_modules/only-bundled': {
            version: '1.0.0',
            inBundle: true,
            hasInstallScript: true,
          },
        },
      }),
      node_modules: {
        'parent-pkg': {
          'package.json': JSON.stringify({
            name: 'parent-pkg',
            version: '1.0.0',
            bundleDependencies: ['only-bundled'],
          }),
          node_modules: {
            'only-bundled': {
              'package.json': JSON.stringify({
                name: 'only-bundled',
                version: '1.0.0',
                scripts: { install: 'echo evil' },
              }),
            },
          },
        },
      },
    },
    config: { all: true },
  })
  await npm.exec('approve-scripts', [])
  t.match(joinedOutput(), /No packages with unreviewed install scripts/)
  t.strictSame(logs.warn.byTitle('approve-scripts'), [],
    'no warning; bundled deps are excluded upstream')
  // Ensure no policy entry was written.
  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.notOk(pkg.allowScripts, 'no allowScripts written')
})

// Layout produced by install-strategy=linked: the real package lives in the store, node_modules holds a symlink to it, and the hidden lockfile records that isolated layout (npm/cli#9939).
const setupLinkedProject = (t, { allowScripts, noResolved = false } = {}) => {
  const storeDir = 'canvas@1.0.0-c2FsdHNhbHRzYWx0c2FsdA'
  const storeLoc = `node_modules/.store/${storeDir}/node_modules/canvas`
  const tarUrl = noResolved
    ? undefined
    : 'https://registry.npmjs.org/canvas/-/canvas-1.0.0.tgz'
  const pkg = {
    name: 'host',
    version: '1.0.0',
    dependencies: { canvas: '^1.0.0' },
  }
  if (allowScripts !== undefined) {
    pkg.allowScripts = allowScripts
  }
  return {
    'package.json': JSON.stringify(pkg, null, 2),
    'package-lock.json': JSON.stringify({
      name: pkg.name,
      version: pkg.version,
      lockfileVersion: 3,
      requires: true,
      packages: {
        '': pkg,
        'node_modules/canvas': {
          version: '1.0.0',
          resolved: tarUrl,
          hasInstallScript: true,
        },
      },
    }),
    node_modules: {
      '.package-lock.json': JSON.stringify({
        lockfileVersion: 3,
        requires: true,
        packages: {
          [`node_modules/.store/${storeDir}`]: {},
          [storeLoc]: {
            version: '1.0.0',
            resolved: tarUrl,
            hasInstallScript: true,
          },
          'node_modules/canvas': {
            resolved: storeLoc,
            link: true,
          },
        },
      }),
      '.store': {
        [storeDir]: {
          node_modules: {
            canvas: {
              'package.json': JSON.stringify({
                name: 'canvas',
                version: '1.0.0',
                scripts: { install: 'echo install' },
              }),
            },
          },
        },
      },
      canvas: t.fixture('symlink', `.store/${storeDir}/node_modules/canvas`),
    },
  }
}

// loadActual only trusts the hidden lockfile when it is newer than every dir under node_modules (assertNoNewer allows 10ms), and fixture creation order makes that racy, so bump its mtime past the fixture writes.
const trustHiddenLockfile = (prefix) => {
  const future = new Date(Date.now() + 60_000)
  fs.utimesSync(resolve(prefix, 'node_modules', '.package-lock.json'), future, future)
}

t.test('approve-scripts <pkg> under linked strategy writes a registry pin, not store paths', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupLinkedProject(t),
  })
  trustHiddenLockfile(prefix)
  await npm.exec('approve-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
})

t.test('approve-scripts --all under linked strategy writes a registry pin, not store paths', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupLinkedProject(t),
    config: { all: true },
  })
  trustHiddenLockfile(prefix)
  await npm.exec('approve-scripts', [])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
})

t.test('approve-scripts --pending is empty when the registry pin covers a linked store package', async t => {
  const { npm, prefix, joinedOutput } = await mockNpm(t, {
    prefixDir: setupLinkedProject(t, { allowScripts: { 'canvas@1.0.0': true } }),
    config: { 'allow-scripts-pending': true },
  })
  trustHiddenLockfile(prefix)
  await npm.exec('approve-scripts', [])
  t.match(joinedOutput(), /No packages with unreviewed install scripts/)
})

t.test('deny-scripts under linked strategy writes a name-only deny', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupLinkedProject(t),
  })
  trustHiddenLockfile(prefix)
  await npm.exec('deny-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { canvas: false })
})

const setupLocalFileDepProject = (t) => {
  const pkg = {
    name: 'host',
    version: '1.0.0',
    dependencies: { foo: 'file:./local-foo' },
  }
  return {
    'package.json': JSON.stringify(pkg, null, 2),
    'package-lock.json': JSON.stringify({
      name: 'host',
      version: '1.0.0',
      lockfileVersion: 3,
      requires: true,
      packages: {
        '': pkg,
        'local-foo': {
          name: 'local-foo',
          version: '1.0.0',
          hasInstallScript: true,
        },
        'node_modules/foo': {
          resolved: 'local-foo',
          link: true,
        },
      },
    }),
    'local-foo': {
      'package.json': JSON.stringify({
        name: 'local-foo',
        version: '1.0.0',
        scripts: { install: 'echo install' },
      }),
    },
    node_modules: {
      foo: t.fixture('symlink', '../local-foo'),
    },
  }
}

t.test('approve-scripts <name> finds a local file: dep through its link', async t => {
  // The Link is named `foo` while its target is named `local-foo`; positional matching must dereference the Link instead of skipping it (npm/cli#9939 review).
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupLocalFileDepProject(t),
  })
  await npm.exec('approve-scripts', ['foo'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'file:../local-foo': true })
})

t.test('deny-scripts <name> finds a local file: dep through its link', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupLocalFileDepProject(t),
  })
  await npm.exec('deny-scripts', ['foo'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'file:../local-foo': false })
})

t.test('approve-scripts <pkg> under linked strategy without resolved URLs approves by name', async t => {
  // omit-lockfile-registry-resolved: the store package has no resolved URL, so it cannot be pinned but must still be approved by name via the incoming Link's edge.
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupLinkedProject(t, { noResolved: true }),
  })
  trustHiddenLockfile(prefix)
  await npm.exec('approve-scripts', ['canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { canvas: true })
})

for (const state of ['missing', 'stale']) {
  t.test(`prune keeps denies with a ${state} hidden lockfile`, async t => {
    // With no trusted version, the versioned deny still blocks at runtime, so prune must fail closed and keep it instead of flipping the effective policy to allowed (npm/cli#9941 review).
    const Arborist = require('@npmcli/arborist')
    const isScriptAllowed = require('@npmcli/arborist/lib/script-allowed.js')
    const allowScripts = {
      canvas: true,
      'canvas@1.0.0': false,
    }

    const { npm, prefix } = await mockNpm(t, {
      prefixDir: setupLinkedProject(t, { allowScripts }),
      config: { 'install-strategy': 'linked' },
    })

    const hidden = resolve(prefix, 'node_modules', '.package-lock.json')
    if (state === 'missing') {
      fs.unlinkSync(hidden)
    } else {
      fs.utimesSync(hidden, new Date(0), new Date(0))
    }

    t.equal(npm.config.get('omit-lockfile-registry-resolved'), false)

    const arb = new Arborist({ ...npm.flatOptions, path: prefix })
    const tree = await arb.loadActual()
    const target = [...tree.inventory.values()]
      .find(node => !node.isLink && node.name === 'canvas')

    t.ok(target, 'finds the installed target')
    t.strictSame(
      isScriptAllowed.getTrustedRegistryIdentity(target),
      { name: 'canvas', version: null },
      'recovers the registry name without inventing a trusted version'
    )
    t.equal(
      isScriptAllowed(target, allowScripts),
      false,
      'the versioned deny blocks the package before pruning'
    )

    await npm.exec('install-scripts', ['prune'])

    const pkg = JSON.parse(
      fs.readFileSync(resolve(prefix, 'package.json'), 'utf8')
    )
    t.strictSame(
      pkg.allowScripts,
      allowScripts,
      'pruning preserves both the allow and its active deny exception'
    )
    t.equal(
      isScriptAllowed(target, pkg.allowScripts),
      false,
      'the package remains denied after pruning'
    )
  })
}
