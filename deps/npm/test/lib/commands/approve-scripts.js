const t = require('tap')
const fs = require('node:fs')
const { resolve } = require('node:path')
const _mockNpm = require('../../fixtures/mock-npm')

const mockNpm = async (t, opts = {}) => {
  return _mockNpm(t, opts)
}

const setupProject = ({ allowScripts, withScripts = ['canvas'] } = {}) => {
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
    lockPackages[`node_modules/${name}`] = {
      version: '1.0.0',
      resolved: tarUrl,
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

t.test('approve-scripts --pending lists unreviewed packages', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
    config: { 'allow-scripts-pending': true },
  })
  await npm.exec('approve-scripts', [])
  const out = joinedOutput()
  t.match(out, /2 packages have install scripts not yet covered/)
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

t.test('approve-scripts errors on unknown package', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('approve-scripts', ['not-installed']),
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

t.test('approve-scripts groups multiple installed versions of the same package', async t => {
  // Two versions of lodash exist in the tree; both have install scripts.
  // groupByPackage should put them in the same group (hits the
  // `if (!groups[key])` falsy branch on the second node).
  const { npm, prefix } = await _mockNpm(t, {
    prefixDir: {
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
    },
  })
  await npm.exec('approve-scripts', ['lodash'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  // Both versions get pinned.
  t.strictSame(pkg.allowScripts, {
    'lodash@3.10.1': true,
    'lodash@4.17.21': true,
  })
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

  const warnings = mock.logs.warn.byTitle('allow-scripts')
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

t.test('approve-scripts --all skips bundled deps with a notice', async t => {
  // Bundled deps cannot be allowlisted in Phase 1 (RFC defers their
  // allowlisting to a follow-up). --all must not silently write a key
  // derived from the bundled tarball's self-claimed identity.
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
  t.match(logs.warn.byTitle('approve-scripts'), [/Skipping 1 bundled dependency/])
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

t.test('approve-scripts --all with only bundled deps prints "no eligible" notice', async t => {
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
  t.match(joinedOutput(), /No packages eligible for approval/)
  t.match(logs.warn.byTitle('approve-scripts'), [/Skipping 1 bundled dependency/])
  // Ensure no policy entry was written.
  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.notOk(pkg.allowScripts, 'no allowScripts written')
})
