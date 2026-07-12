const t = require('tap')
const fs = require('node:fs')
const { resolve } = require('node:path')
const _mockNpm = require('../../fixtures/mock-npm')
const InstallScripts = require('../../../lib/commands/install-scripts.js')

const mockNpm = async (t, opts = {}) => {
  return _mockNpm(t, opts)
}

const setupProject = ({ allowScripts, withScripts = ['canvas'], noScripts = [] } = {}) => {
  const pkg = {
    name: 'host',
    version: '1.0.0',
    dependencies: Object.fromEntries([...withScripts, ...noScripts].map((n) => [n, '*'])),
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
      hasInstallScript: true,
      resolved: `https://registry.npmjs.org/${name}/-/${name}-1.0.0.tgz`,
    }
  }
  for (const name of noScripts) {
    nodeModules[name] = {
      'package.json': JSON.stringify({ name, version: '1.0.0' }),
    }
    lockPackages[`node_modules/${name}`] = {
      version: '1.0.0',
      resolved: `https://registry.npmjs.org/${name}/-/${name}-1.0.0.tgz`,
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

t.test('completion', async t => {
  const comp = (argv) =>
    InstallScripts.completion({ conf: { argv: { remain: argv } } })

  t.resolveMatch(comp(['npm', 'install-scripts']), ['approve', 'deny', 'ls', 'prune'])
  t.resolveMatch(comp(['npm', 'install-scripts', 'approve']), [])
  t.resolveMatch(comp(['npm', 'install-scripts', 'deny']), [])
  t.resolveMatch(comp(['npm', 'install-scripts', 'ls']), [])
  t.resolveMatch(comp(['npm', 'install-scripts', 'prune']), [])
  await t.rejects(comp(['npm', 'install-scripts', 'frobnicate']), {
    message: 'frobnicate not recognized',
  })
})

t.test('install-scripts approve <pkg> writes a pinned entry', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await npm.exec('install-scripts', ['approve', 'canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
})

t.test('install-scripts approve --all approves every unreviewed package', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
    config: { all: true },
  })
  await npm.exec('install-scripts', ['approve'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, {
    'canvas@1.0.0': true,
    'sharp@1.0.0': true,
  })
})

t.test('install-scripts deny <pkg> writes a name-only false entry', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await npm.exec('install-scripts', ['deny', 'canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { canvas: false })
})

t.test('install-scripts deny --all denies every unreviewed package', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
    config: { all: true },
  })
  await npm.exec('install-scripts', ['deny'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { canvas: false, sharp: false })
})

t.test('install-scripts ignores allow-scripts-pending and still writes', async t => {
  // The namespace exposes listing through `ls`, so a stray
  // `allow-scripts-pending` config must not divert approve into list mode.
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { 'allow-scripts-pending': true },
  })
  await npm.exec('install-scripts', ['approve', 'canvas'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
})

t.test('install-scripts ls lists unreviewed packages', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas', 'sharp'] }),
  })
  await npm.exec('install-scripts', ['ls'])
  const out = joinedOutput()
  t.match(out, /2 packages have install scripts not yet covered by allowScripts/)
  t.match(out, /canvas@1\.0\.0/)
  t.match(out, /sharp@1\.0\.0/)
})

t.test('install-scripts ls with no unreviewed says so', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ allowScripts: { canvas: true }, withScripts: ['canvas'] }),
  })
  await npm.exec('install-scripts', ['ls'])
  t.match(joinedOutput(), /No packages with unreviewed install scripts/)
})

t.test('install-scripts ls rejects positional args', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('install-scripts', ['ls', 'canvas']),
    /cannot be combined with positional arguments/
  )
})

t.test('install-scripts with no subcommand errors with usage', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('install-scripts', []),
    { code: 'EUSAGE' }
  )
})

t.test('install-scripts with an unknown subcommand errors', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('install-scripts', ['frobnicate']),
    /`frobnicate` is not a recognized subcommand/
  )
})

t.test('install-scripts approve errors on unknown package', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('install-scripts', ['approve', 'not-installed']),
    { code: 'ENOMATCH' }
  )
})

t.test('install-scripts fails for global installs', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { global: true },
  })
  await t.rejects(
    npm.exec('install-scripts', ['approve', 'canvas']),
    { code: 'EGLOBAL' }
  )
})

t.test('install-scripts prune removes not-installed and no-script entries', async t => {
  const { npm, prefix, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      noScripts: ['no-scripts-pkg'],
      allowScripts: {
        'canvas@1.0.0': true,
        'no-scripts-pkg': true,
        gone: true,
      },
    }),
  })
  await npm.exec('install-scripts', ['prune'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })

  const out = joinedOutput()
  t.match(out, /Removed 2 unused allowScripts entries:/)
  t.match(out, /no-scripts-pkg \(no install scripts\)/)
  t.match(out, /gone \(package not installed\)/)
})

t.test('install-scripts prune removes unused deny entries too', async t => {
  const { npm, prefix } = await mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      allowScripts: { 'canvas@1.0.0': true, 'denied-gone': false },
    }),
  })
  await npm.exec('install-scripts', ['prune'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
})

t.test('install-scripts prune removes a stale version pin and drops the field', async t => {
  const { npm, prefix, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      allowScripts: { 'canvas@9.9.9': true },
    }),
  })
  await npm.exec('install-scripts', ['prune'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.notOk('allowScripts' in pkg, 'allowScripts field is removed when empty')
  // Singular wording for a single entry.
  t.match(joinedOutput(), /Removed 1 unused allowScripts entry:/)
})

t.test('install-scripts prune --dry-run reports without writing', async t => {
  const allowScripts = { 'canvas@1.0.0': true, gone: true }
  const { npm, prefix, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'], allowScripts }),
    config: { 'dry-run': true },
  })
  await npm.exec('install-scripts', ['prune'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, allowScripts, 'package.json is unchanged')
  t.match(joinedOutput(), /Would remove 1 unused allowScripts entry:/)
})

t.test('install-scripts prune --json emits a machine-readable summary', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      allowScripts: { 'canvas@1.0.0': true, gone: true },
    }),
    config: { json: true },
  })
  await npm.exec('install-scripts', ['prune'])

  t.strictSame(JSON.parse(joinedOutput()), {
    allowScripts: {
      removed: [{ key: 'gone', value: true, reason: 'not-installed' }],
      dryRun: false,
    },
  })
})

t.test('install-scripts prune with nothing unused says so', async t => {
  const { npm, prefix, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({
      withScripts: ['canvas'],
      allowScripts: { 'canvas@1.0.0': true },
    }),
  })
  await npm.exec('install-scripts', ['prune'])

  const pkg = JSON.parse(fs.readFileSync(resolve(prefix, 'package.json'), 'utf8'))
  t.strictSame(pkg.allowScripts, { 'canvas@1.0.0': true })
  t.match(joinedOutput(), /No unused allowScripts entries\./)
})

t.test('install-scripts prune with no allowScripts field says so', async t => {
  const { npm, joinedOutput } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await npm.exec('install-scripts', ['prune'])
  t.match(joinedOutput(), /No unused allowScripts entries\./)
})

t.test('install-scripts prune rejects positional args', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
  })
  await t.rejects(
    npm.exec('install-scripts', ['prune', 'canvas']),
    /cannot be combined with positional arguments/
  )
})

t.test('install-scripts prune rejects --all', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { all: true },
  })
  await t.rejects(
    npm.exec('install-scripts', ['prune']),
    /cannot be combined with positional arguments or `--all`/
  )
})

t.test('install-scripts prune fails for global installs', async t => {
  const { npm } = await mockNpm(t, {
    prefixDir: setupProject({ withScripts: ['canvas'] }),
    config: { global: true },
  })
  await t.rejects(
    npm.exec('install-scripts', ['prune']),
    { code: 'EGLOBAL' }
  )
})
