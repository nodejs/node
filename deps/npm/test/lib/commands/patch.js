const fs = require('node:fs')
const path = require('node:path')
const t = require('tap')
const Arborist = require('@npmcli/arborist')
const pacote = require('pacote')

const { loadNpmWithRegistry: loadMockNpm } = require('../../fixtures/mock-npm')
const Patch = require('../../../lib/commands/patch.js')

// Tiny dependency served by the mock registry so pacote can extract it.
const DEP_NAME = 'patch-me'
const DEP_VERSION = '1.0.0'
const DEP_SRC = 'module.exports = function () { return "original" }\n'

// On-disk tarball contents for the dependency.
const depTarball = {
  'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }),
  'index.js': DEP_SRC,
}

// Root project package.json depending on the patchable dep.
const rootPackageJson = {
  name: 'root-project',
  version: '1.0.0',
  dependencies: { [DEP_NAME]: `^${DEP_VERSION}` },
}

// Lockfile pre-resolving the dep so installs/reifies are deterministic.
const rootPackageLock = {
  name: 'root-project',
  version: '1.0.0',
  lockfileVersion: 3,
  requires: true,
  packages: {
    '': {
      name: 'root-project',
      version: '1.0.0',
      dependencies: { [DEP_NAME]: `^${DEP_VERSION}` },
    },
    [`node_modules/${DEP_NAME}`]: {
      version: DEP_VERSION,
      resolved: `https://registry.npmjs.org/${DEP_NAME}/-/${DEP_NAME}-${DEP_VERSION}.tgz`,
    },
  },
}

// Persist the manifest and tarball so the many extract and reify passes (add, commit baseline, reify, rm reify, install) all find a tarball without having to count requests precisely.
const setupDep = async (npm, registry) => {
  const manifest = registry.manifest({ name: DEP_NAME, versions: [DEP_VERSION] })
  const dist = new URL(manifest.versions[DEP_VERSION].dist.tarball)
  const tar = await pacote.tarball(path.join(npm.prefix, 'dep-tarball'), { Arborist })
  registry.nock.get(`/${DEP_NAME}`).reply(200, manifest).persist()
  registry.nock.get(dist.pathname).reply(200, tar).persist()
  return manifest
}

const basePrefix = () => ({
  'dep-tarball': depTarball,
  'package.json': JSON.stringify(rootPackageJson),
  'package-lock.json': JSON.stringify(rootPackageLock),
})

const readJson = file => JSON.parse(fs.readFileSync(file, 'utf8'))

t.test('no args rejects with EUSAGE', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: basePrefix(),
  })
  await t.rejects(npm.exec('patch', []), { code: 'EUSAGE' }, 'bare npm patch is a usage error')
})

t.test('add with no pkg rejects with EUSAGE', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: basePrefix(),
  })
  await t.rejects(npm.exec('patch', ['add']), { code: 'EUSAGE' })
})

t.test('add rejects non-registry spec with EPATCHNONREGISTRY', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: basePrefix(),
  })
  await t.rejects(
    npm.exec('patch', ['add', 'file:./dep-tarball']),
    { code: 'EPATCHNONREGISTRY' },
    'file: spec is rejected'
  )
})

t.test('add accepts an edgeless installed node (extraneous / linked store)', async t => {
  // an installed-but-undeclared dep has no edges, so isRegistryDependency is false;
  // it must not be misread as non-registry the way a linked store node or extraneous install would be
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: {
      'dep-tarball': depTarball,
      'package.json': JSON.stringify({ name: 'root-project', version: '1.0.0' }),
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
      },
    },
  })
  await setupDep(npm, registry)
  await npm.exec('patch', ['add', DEP_NAME])
  t.match(joinedOutput(), /You can now edit the following directory: /, 'edgeless node is patchable')
})

t.test('full round-trip: install, add, edit, commit, ls, rm', async t => {
  const { npm, joinedOutput, registry, outputs } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)

  // install the dep so it is present on disk
  await npm.exec('install', [])
  const installedIndex = path.join(npm.prefix, 'node_modules', DEP_NAME, 'index.js')
  t.equal(fs.readFileSync(installedIndex, 'utf8'), DEP_SRC, 'installed clean')

  // npm patch add <dep> prints the edit dir and commit hint
  outputs.length = 0
  await npm.exec('patch', ['add', DEP_NAME])
  const addOut = joinedOutput()
  t.match(addOut, /You can now edit the following directory: /, 'prints edit dir line')
  t.match(addOut, /When done, run: npm patch commit /, 'prints commit hint line')

  const editDirMatch = addOut.match(/You can now edit the following directory: (.+)/)
  const editDir = editDirMatch[1].trim()
  t.ok(fs.existsSync(path.join(editDir, 'package.json')), 'extracted package.json to edit dir')

  // edit a file in the printed edit dir
  const edited = 'module.exports = function () { return "patched" }\n'
  fs.writeFileSync(path.join(editDir, 'index.js'), edited)

  // npm patch commit <dir>
  outputs.length = 0
  await npm.exec('patch', ['commit', editDir])

  // patches/<dep>@<ver>.patch exists
  const patchFile = path.join(npm.prefix, 'patches', `${DEP_NAME}@${DEP_VERSION}.patch`)
  t.ok(fs.existsSync(patchFile), 'patch file written under patches/')
  t.match(fs.readFileSync(patchFile, 'utf8'), /patched/, 'patch file contains the edit')

  // package.json has the relative patchedDependencies entry
  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  t.same(
    pkg.patchedDependencies,
    { [`${DEP_NAME}@${DEP_VERSION}`]: `patches/${DEP_NAME}@${DEP_VERSION}.patch` },
    'patchedDependencies has the relative posix entry'
  )

  // package-lock.json: lockfileVersion 4 and packages[node_modules/<dep>].patched
  const lock = readJson(path.join(npm.prefix, 'package-lock.json'))
  t.equal(lock.lockfileVersion, 4, 'lockfile bumped to v4')
  const lockNode = lock.packages[`node_modules/${DEP_NAME}`]
  t.ok(lockNode.patched, 'lockfile node has patched block')
  t.equal(lockNode.patched.path, `patches/${DEP_NAME}@${DEP_VERSION}.patch`, 'patched.path set')
  t.match(lockNode.patched.integrity, /^sha512-/, 'patched.integrity is an SSRI')

  // the installed file on disk contains the edit
  t.equal(fs.readFileSync(installedIndex, 'utf8'), edited, 'installed file is patched on disk')

  // edit dir removed by default
  t.notOk(fs.existsSync(editDir), 'edit dir removed when keep-edit-dir not set')

  // npm patch ls lists the entry
  outputs.length = 0
  await npm.exec('patch', ['ls'])
  const lsOut = joinedOutput()
  t.match(lsOut, new RegExp(`patches/${DEP_NAME}@${DEP_VERSION}\\.patch`), 'ls shows patch path')
  t.match(lsOut, new RegExp(`${DEP_NAME}@${DEP_VERSION}`), 'ls shows selector')
  t.match(lsOut, /\(1 node\)/, 'ls shows node count')

  // npm patch rm removes the entry from package.json and deletes the file
  outputs.length = 0
  await npm.exec('patch', ['rm', DEP_NAME])
  const pkgAfter = readJson(path.join(npm.prefix, 'package.json'))
  t.notOk(pkgAfter.patchedDependencies, 'patchedDependencies removed from package.json')
  t.notOk(fs.existsSync(patchFile), 'patch file deleted')

  // rm clears the patch record from the lockfile and reverts the installed file
  const lockAfter = readJson(path.join(npm.prefix, 'package-lock.json'))
  t.notOk(
    lockAfter.packages[`node_modules/${DEP_NAME}`].patched,
    'lockfile patched block removed'
  )
  t.equal(
    fs.readFileSync(installedIndex, 'utf8'),
    DEP_SRC,
    'installed file reverted to original'
  )
})

t.test('bare form routes to add', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  // npm patch <dep> behaves like npm patch add <dep>
  await npm.exec('patch', [DEP_NAME])
  t.match(joinedOutput(), /You can now edit the following directory: /, 'bare form extracts like add')
})

t.test('npm ci rejects patch path drift from the lockfile', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  // commit a real patch so the lockfile records patched.path
  const editDir = path.join(npm.prefix, 'edit')
  await pacote.extract(`${DEP_NAME}@${DEP_VERSION}`, editDir, npm.flatOptions)
  fs.writeFileSync(path.join(editDir, 'index.js'), 'module.exports = () => "patched"\n')
  await npm.exec('patch', ['commit', editDir])

  // move the patch file and repoint package.json without updating the lockfile
  const pkgPath = path.join(npm.prefix, 'package.json')
  const pkg = readJson(pkgPath)
  const key = `${DEP_NAME}@${DEP_VERSION}`
  const oldPath = path.join(npm.prefix, pkg.patchedDependencies[key])
  const newRel = 'patches/renamed.patch'
  fs.renameSync(oldPath, path.join(npm.prefix, newRel))
  pkg.patchedDependencies[key] = newRel
  fs.writeFileSync(pkgPath, JSON.stringify(pkg))

  await t.rejects(
    npm.exec('ci', []),
    /package-lock\.json are in sync/,
    'npm ci refuses when the patch path diverges from the lockfile'
  )
})

t.test('rm with no registered patch rejects with EPATCHNOTFOUND', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: basePrefix(),
  })
  await t.rejects(
    npm.exec('patch', ['rm', DEP_NAME]),
    { code: 'EPATCHNOTFOUND' },
    'rm errors when nothing matches'
  )
})

t.test('ls with no patches prints nothing', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: basePrefix(),
  })
  await npm.exec('patch', ['ls'])
  t.equal(joinedOutput(), '', 'no output when no patchedDependencies')
})

t.test('ls with no package.json prints nothing', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {},
  })
  await npm.exec('patch', ['ls'])
  t.equal(joinedOutput(), '', 'no output and no crash without a package.json')
})

t.test('add with edit-dir config uses that directory', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  const customDir = path.join(npm.prefix, 'my-edit-dir')
  npm.config.set('edit-dir', customDir)
  await npm.exec('patch', ['add', DEP_NAME])
  t.match(joinedOutput(), new RegExp('my-edit-dir'), 'uses configured edit dir')
  t.ok(fs.existsSync(path.join(customDir, 'package.json')), 'extracted into configured dir')
})

t.test('add: not-installed bare name rejects with EPATCHNOTINSTALLED', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({ name: 'root-project', version: '1.0.0' }),
    },
  })
  await t.rejects(
    npm.exec('patch', ['add', DEP_NAME]),
    { code: 'EPATCHNOTINSTALLED' },
    'errors when no installed version and no explicit version'
  )
})

t.test('add: ambiguous when multiple versions installed', async t => {
  // root-direct 1.0.0 plus two nested 2.0.0 copies, so the dedup guard and the root-dependant label are both exercised while listing the ambiguity
  const nestedDep = v => ({
    node_modules: { [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: v }) } },
  })
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        dependencies: { [DEP_NAME]: '1.0.0', b: '1.0.0', c: '1.0.0' },
      }),
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '1.0.0' }) },
        b: {
          'package.json': JSON.stringify({ name: 'b', version: '1.0.0', dependencies: { [DEP_NAME]: '2.0.0' } }),
          ...nestedDep('2.0.0'),
        },
        c: {
          'package.json': JSON.stringify({ name: 'c', version: '1.0.0', dependencies: { [DEP_NAME]: '2.0.0' } }),
          ...nestedDep('2.0.0'),
        },
      },
    },
  })
  await t.rejects(
    npm.exec('patch', ['add', DEP_NAME]),
    { code: 'EPATCHAMBIGUOUS' },
    'errors when multiple versions are installed for a bare name'
  )
})

t.test('add: an installed file: dependency is rejected as non-registry', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project', version: '1.0.0', dependencies: { [DEP_NAME]: 'file:./local' },
      }),
      local: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
      },
    },
  })
  await t.rejects(
    npm.exec('patch', ['add', DEP_NAME]),
    { code: 'EPATCHNONREGISTRY' },
    'cannot patch a file: dependency that is already installed'
  )
})

t.test('add: a version installed as both registry and file: is rejected', async t => {
  // one consumer pulls the registry copy, another pulls a file: copy of the same version;
  // the file: edge must still cause a rejection even though a registry edge also exists
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        dependencies: { [DEP_NAME]: '1.0.0', b: '1.0.0' },
      }),
      local: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
        b: {
          'package.json': JSON.stringify({
            name: 'b', version: '1.0.0', dependencies: { [DEP_NAME]: 'file:../../local' },
          }),
          node_modules: {
            [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
          },
        },
      },
    },
  })
  await t.rejects(
    npm.exec('patch', ['add', DEP_NAME]),
    { code: 'EPATCHNONREGISTRY' },
    'a version with any file: consumer cannot be patched'
  )
})

t.test('add: a range matching multiple installed versions is ambiguous', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        dependencies: { [DEP_NAME]: '1.0.0', b: '1.0.0' },
      }),
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '1.0.0' }) },
        b: {
          'package.json': JSON.stringify({ name: 'b', version: '1.0.0', dependencies: { [DEP_NAME]: '2.0.0' } }),
          node_modules: { [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '2.0.0' }) } },
        },
      },
    },
  })
  await t.rejects(
    npm.exec('patch', ['add', `${DEP_NAME}@>=1.0.0`]),
    { code: 'EPATCHAMBIGUOUS' },
    'a range matching two installed versions errors'
  )
})

t.test('add: explicit exact version is honored without install', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  // no install; explicit exact version path returns { name, version } directly
  await npm.exec('patch', ['add', `${DEP_NAME}@${DEP_VERSION}`])
  t.match(joinedOutput(), /You can now edit the following directory: /, 'extracts the exact version')
})

t.test('commit: no edit dir arg rejects with EUSAGE', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: basePrefix(),
  })
  await t.rejects(npm.exec('patch', ['commit']), { code: 'EUSAGE' })
})

t.test('commit: missing package.json in edit dir rejects with EPATCHNOEDITDIR', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: { 'package.json': JSON.stringify(rootPackageJson), 'empty-dir': {} },
  })
  await t.rejects(
    npm.exec('patch', ['commit', path.join(npm.prefix, 'empty-dir')]),
    { code: 'EPATCHNOEDITDIR' }
  )
})

t.test('commit: no changes logs a warning and does not write a patch', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  // add then commit without editing anything
  await npm.exec('patch', ['add', DEP_NAME])
  // the edit dir is a tmp path; re-extract a fresh clean copy to a known dir
  const editDir = path.join(npm.prefix, 'clean-edit')
  await pacote.extract(`${DEP_NAME}@${DEP_VERSION}`, editDir, npm.flatOptions)

  await npm.exec('patch', ['commit', editDir])
  t.notOk(
    fs.existsSync(path.join(npm.prefix, 'patches', `${DEP_NAME}@${DEP_VERSION}.patch`)),
    'no patch file written when there are no changes'
  )
  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  t.notOk(pkg.patchedDependencies, 'no patchedDependencies added when nothing changed')
})

t.test('commit: only package.json changed warns and writes no patch', async t => {
  const { npm, logs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  await npm.exec('patch', ['add', DEP_NAME])
  const editDir = path.join(npm.prefix, 'clean-edit')
  await pacote.extract(`${DEP_NAME}@${DEP_VERSION}`, editDir, npm.flatOptions)

  // edit only package.json, which is excluded from patches
  const pkgPath = path.join(editDir, 'package.json')
  const edited = readJson(pkgPath)
  edited.description = 'edited'
  fs.writeFileSync(pkgPath, JSON.stringify(edited))

  await npm.exec('patch', ['commit', editDir])
  t.notOk(
    fs.existsSync(path.join(npm.prefix, 'patches', `${DEP_NAME}@${DEP_VERSION}.patch`)),
    'no patch file written when only package.json changed'
  )
  t.match(logs.warn.join('\n'), /only package.json changed/, 'warns package.json is not patchable')
})

t.test('commit: package.json change alongside code is dropped with a warning', async t => {
  const { npm, logs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  await npm.exec('patch', ['add', DEP_NAME])
  const editDir = path.join(npm.prefix, 'clean-edit')
  await pacote.extract(`${DEP_NAME}@${DEP_VERSION}`, editDir, npm.flatOptions)

  // edit both package.json and a real file
  const pkgPath = path.join(editDir, 'package.json')
  const edited = readJson(pkgPath)
  edited.description = 'edited'
  fs.writeFileSync(pkgPath, JSON.stringify(edited))
  fs.writeFileSync(path.join(editDir, 'index.js'), 'module.exports = () => "patched"\n')

  await npm.exec('patch', ['commit', editDir])
  const patchPath = path.join(npm.prefix, 'patches', `${DEP_NAME}@${DEP_VERSION}.patch`)
  t.ok(fs.existsSync(patchPath), 'patch written for the code change')
  t.notMatch(fs.readFileSync(patchPath, 'utf8'), 'package.json', 'patch excludes package.json')
  t.match(
    logs.warn.join('\n'),
    /changes to package.json are not included/,
    'warns the package.json edit was ignored'
  )
})

// Serve several versions of a package, each with its own index.js source.
const setupVersions = async (npm, registry, name, sources) => {
  const versions = Object.keys(sources)
  const manifest = registry.manifest({ name, versions })
  for (const version of versions) {
    const dir = path.join(npm.prefix, `pkg-${name}-${version}`)
    fs.mkdirSync(dir, { recursive: true })
    fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify({ name, version }))
    fs.writeFileSync(path.join(dir, 'index.js'), sources[version])
    const tar = await pacote.tarball(dir, { Arborist })
    const { pathname } = new URL(manifest.versions[version].dist.tarball)
    registry.nock.get(pathname).reply(200, tar).persist()
  }
  registry.nock.get(`/${name}`).reply(200, manifest).persist()
  return manifest
}

const rootWith = dep => ({
  'package.json': JSON.stringify({
    name: 'root-project', version: '1.0.0', dependencies: dep,
  }),
})

const updatePrefix = patchedDependencies => ({
  'package.json': JSON.stringify({
    name: 'root-project', version: '1.0.0', patchedDependencies,
  }),
})

t.test('update --to rebases an exact patch onto a new version', async t => {
  const name = 'upd-exact'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  // v2 differs from v1 only on the last line; the patch edits the first line -> clean 3-way merge
  await setupVersions(npm, registry, name, { '1.0.0': 'a\nb\nc\n', '2.0.0': 'a\nb\nCC\n' })
  await npm.exec('install', [])

  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const editDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(editDir, 'index.js'), 'AA\nb\nc\n')
  await npm.exec('patch', ['commit', editDir])

  npm.config.set('to', '2.0.0')
  await npm.exec('patch', ['update', name])

  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  t.same(pkg.patchedDependencies, { [`${name}@2.0.0`]: `patches/${name}@2.0.0.patch` },
    'selector renamed to the new version')
  t.notOk(fs.existsSync(path.join(npm.prefix, 'patches', `${name}@1.0.0.patch`)), 'old patch file removed')
  t.match(fs.readFileSync(path.join(npm.prefix, 'patches', `${name}@2.0.0.patch`), 'utf8'), /\+AA/,
    'rebased patch keeps the edit')
})

t.test('update --to warns when the target version is not installed', async t => {
  const name = 'upd-uninstalled'
  const { npm, joinedOutput, outputs, registry, logs } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'a\nb\nc\n', '2.0.0': 'a\nb\nCC\n' })
  await npm.exec('install', [])

  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const editDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(editDir, 'index.js'), 'AA\nb\nc\n')
  await npm.exec('patch', ['commit', editDir])

  // dependency is pinned to 1.0.0, so rebasing onto 2.0.0 targets an uninstalled version
  npm.config.set('to', '2.0.0')
  await npm.exec('patch', ['update', name])

  t.match(logs.warn.byTitle('patch'),
    [new RegExp(`${name}@2\\.0\\.0 is not installed.*EPATCHUNUSED`)],
    'warns that the target version is not installed')
})

t.test('update --to is silent when the target version is installed', async t => {
  const name = 'upd-installed'
  const { npm, registry, logs } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^2.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'x\n', '2.0.0': 'x\n' })
  // 2.0.0 is the installed version; hand-author a patch against 1.0.0 that adds a file (applies to any version)
  await npm.exec('install', [])
  fs.mkdirSync(path.join(npm.prefix, 'patches'), { recursive: true })
  fs.writeFileSync(path.join(npm.prefix, 'patches', `${name}@1.0.0.patch`),
    '--- /dev/null\t\n+++ b/EXTRA.txt\t\n@@ -0,0 +1 @@\n+extra\n')
  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  pkg.patchedDependencies = { [`${name}@1.0.0`]: `patches/${name}@1.0.0.patch` }
  fs.writeFileSync(path.join(npm.prefix, 'package.json'), JSON.stringify(pkg))

  // rebasing onto 2.0.0, which is installed, must not warn
  npm.config.set('to', '2.0.0')
  await npm.exec('patch', ['update', name])
  t.strictSame(logs.warn.byTitle('patch'), [], 'no warning when --to matches the installed version')
})

t.test('update auto-detects the new version and drops a fully-shadowed range', async t => {
  const name = 'upd-range'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'x\n', '1.1.0': 'x\n' })
  await npm.exec('install', [])
  // a patch that adds a file applies to any version, so the dep can float
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const editDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(editDir, 'EXTRA.txt'), 'extra\n')
  await npm.exec('patch', ['commit', editDir])

  // turn the exact selector into a range and float the lockfile to 1.1.0
  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  pkg.dependencies[name] = '^1.0.0'
  pkg.patchedDependencies = { [`${name}@^1.0.0`]: pkg.patchedDependencies[`${name}@1.0.0`] }
  fs.writeFileSync(path.join(npm.prefix, 'package.json'), JSON.stringify(pkg))
  // clear the resolved tree so a fresh install floats the range up to 1.1.0
  fs.rmSync(path.join(npm.prefix, 'package-lock.json'))
  fs.rmSync(path.join(npm.prefix, 'node_modules'), { recursive: true, force: true })
  await npm.exec('install', [])

  await npm.exec('patch', ['update', name])
  t.same(readJson(path.join(npm.prefix, 'package.json')).patchedDependencies,
    { [`${name}@1.1.0`]: `patches/${name}@1.1.0.patch` }, 'shadowed range dropped, new exact entry added')
})

t.test('update conflict leaves an edit dir; commit finalizes the rename', async t => {
  const name = 'upd-conflict'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  // v2 changes the same line the patch edits -> conflict
  await setupVersions(npm, registry, name, { '1.0.0': 'a\nb\nc\n', '2.0.0': 'a\nBB\nc\n' })
  await npm.exec('install', [])
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const addDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(addDir, 'index.js'), 'a\nMINE\nc\n')
  await npm.exec('patch', ['commit', addDir])

  npm.config.set('to', '2.0.0')
  outputs.length = 0
  await npm.exec('patch', ['update', name])
  const editDir = joinedOutput().match(/Resolve the conflicts in: (.+)/)[1].trim()
  t.ok(fs.existsSync(path.join(editDir, '.npm-patch-update.json')), 'cleanup marker written')
  t.match(fs.readFileSync(path.join(editDir, 'index.js'), 'utf8'), /<<<<<<</, 'conflict markers present')
  t.same(readJson(path.join(npm.prefix, 'package.json')).patchedDependencies,
    { [`${name}@1.0.0`]: `patches/${name}@1.0.0.patch` }, 'manifest unchanged on conflict')

  // resolve by keeping our line, then commit
  let src = fs.readFileSync(path.join(editDir, 'index.js'), 'utf8')
  src = src.replace(/<<<<<<<[^\n]*\n[\s\S]*?=======\n([\s\S]*?)>>>>>>>[^\n]*\n/, '$1')
  fs.writeFileSync(path.join(editDir, 'index.js'), src)
  await npm.exec('patch', ['commit', editDir])
  t.same(readJson(path.join(npm.prefix, 'package.json')).patchedDependencies,
    { [`${name}@2.0.0`]: `patches/${name}@2.0.0.patch` }, 'renamed after the resolving commit')
  t.notOk(fs.existsSync(path.join(npm.prefix, 'patches', `${name}@1.0.0.patch`)), 'old patch file removed')
})

t.test('a no-op resolving commit keeps the marker so a corrected retry still finalizes', async t => {
  const name = 'upd-noop-retry'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'a\nb\nc\n', '2.0.0': 'a\nBB\nc\n' })
  await npm.exec('install', [])
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const addDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(addDir, 'index.js'), 'a\nMINE\nc\n')
  await npm.exec('patch', ['commit', addDir])

  npm.config.set('to', '2.0.0')
  outputs.length = 0
  await npm.exec('patch', ['update', name])
  const editDir = joinedOutput().match(/Resolve the conflicts in: (.+)/)[1].trim()
  const markerPath = path.join(editDir, '.npm-patch-update.json')
  t.ok(fs.existsSync(markerPath), 'marker written on conflict')

  // resolve to the new version verbatim (no net change) and commit: a no-op
  let src = fs.readFileSync(path.join(editDir, 'index.js'), 'utf8')
  src = src.replace(/<<<<<<<[^\n]*\n([\s\S]*?)=======\n[\s\S]*?>>>>>>>[^\n]*\n/, '$1')
  fs.writeFileSync(path.join(editDir, 'index.js'), src)
  await npm.exec('patch', ['commit', editDir])
  t.ok(fs.existsSync(markerPath), 'marker survives a no-op commit so the update context is not lost')

  // now resolve properly and commit again: must finalize the rename, not throw EPATCHUNUSED on the uninstalled 2.0.0
  fs.writeFileSync(path.join(editDir, 'index.js'), src.replace('BB', 'MINE'))
  await npm.exec('patch', ['commit', editDir])
  t.same(readJson(path.join(npm.prefix, 'package.json')).patchedDependencies,
    { [`${name}@2.0.0`]: `patches/${name}@2.0.0.patch` }, 'finalized: old selector dropped, new one added')
})

t.test('update conflict on a name-only selector forks and commits without EPATCHUNUSED', async t => {
  const name = 'upd-rconflict'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'a\nb\nc\n', '2.0.0': 'a\nBB\nc\n' })
  await npm.exec('install', [])
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const addDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(addDir, 'index.js'), 'a\nMINE\nc\n')
  await npm.exec('patch', ['commit', addDir])
  // turn it into a name-only selector so the conflict path takes the fork (non-exact) branch
  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  pkg.patchedDependencies = { [name]: pkg.patchedDependencies[`${name}@1.0.0`] }
  fs.writeFileSync(path.join(npm.prefix, 'package.json'), JSON.stringify(pkg))

  // --to 2.0.0 is not installed and conflicts; the fork must still leave a marker
  npm.config.set('to', '2.0.0')
  outputs.length = 0
  await npm.exec('patch', ['update', name])
  const editDir = joinedOutput().match(/Resolve the conflicts in: (.+)/)[1].trim()
  t.same(readJson(path.join(editDir, '.npm-patch-update.json')), { name, removeKey: null },
    'a fork still writes a marker, with removeKey null')

  // resolve and commit: must finalize metadata-only, not fail with EPATCHUNUSED on the uninstalled 2.0.0
  let src = fs.readFileSync(path.join(editDir, 'index.js'), 'utf8')
  src = src.replace(/<<<<<<<[^\n]*\n[\s\S]*?=======\n([\s\S]*?)>>>>>>>[^\n]*\n/, '$1')
  fs.writeFileSync(path.join(editDir, 'index.js'), src)
  await npm.exec('patch', ['commit', editDir])

  const after = readJson(path.join(npm.prefix, 'package.json')).patchedDependencies
  t.ok(after[name], 'the name-only selector is kept')
  t.ok(after[`${name}@2.0.0`], 'the new exact selector is added')
})

t.test('update: no registered patch rejects with EPATCHNOTFOUND', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: { 'package.json': JSON.stringify({ name: 'r', version: '1.0.0' }) },
  })
  await t.rejects(npm.exec('patch', ['update', 'nope']), { code: 'EPATCHNOTFOUND' })
})

t.test('update: an unknown explicit selector rejects with EPATCHNOTFOUND', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: updatePrefix({ 'foo@1.0.0': 'patches/foo@1.0.0.patch' }),
  })
  await t.rejects(npm.exec('patch', ['update', 'foo@9.9.9']), { code: 'EPATCHNOTFOUND' })
})

t.test('update: multiple entries for a bare name reject with EPATCHAMBIGUOUS', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: updatePrefix({ 'foo@1.0.0': 'patches/foo@1.0.0.patch', 'foo@2.0.0': 'patches/foo@2.0.0.patch' }),
  })
  await t.rejects(npm.exec('patch', ['update', 'foo']), { code: 'EPATCHAMBIGUOUS' })
})

t.test('update: an unparseable patch filename rejects with EPATCHBASE', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: updatePrefix({ 'foo@1.0.0': 'patches/custom.patch' }),
  })
  await t.rejects(npm.exec('patch', ['update', 'foo@1.0.0']), { code: 'EPATCHBASE' })
})

t.test('update: --to equal to the baseline rejects with EPATCHNOOP', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: updatePrefix({ 'foo@1.0.0': 'patches/foo@1.0.0.patch' }),
  })
  npm.config.set('to', '1.0.0')
  await t.rejects(npm.exec('patch', ['update', 'foo@1.0.0']), { code: 'EPATCHNOOP' })
})

t.test('update: an invalid --to rejects with EPATCHBADTO', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: updatePrefix({ 'foo@1.0.0': 'patches/foo@1.0.0.patch' }),
  })
  npm.config.set('to', 'not-a-version')
  await t.rejects(npm.exec('patch', ['update', 'foo@1.0.0']), { code: 'EPATCHBADTO' })
})

t.test('update: an existing target entry rejects with EPATCHEXISTS', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: updatePrefix({ 'foo@1.0.0': 'patches/foo@1.0.0.patch', 'foo@2.0.0': 'patches/foo@2.0.0.patch' }),
  })
  npm.config.set('to', '2.0.0')
  await t.rejects(npm.exec('patch', ['update', 'foo@1.0.0']), { code: 'EPATCHEXISTS' })
})

t.test('update: a missing lockfile with no --to rejects with EPATCHSTALE', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: updatePrefix({ 'foo@^1.0.0': 'patches/foo@1.0.0.patch' }),
  })
  await t.rejects(npm.exec('patch', ['update', 'foo']),
    { code: 'EPATCHSTALE', message: /could not read the lockfile/ })
})

t.test('update: wrong arg count rejects with EUSAGE', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: { 'package.json': JSON.stringify({ name: 'r', version: '1.0.0' }) },
  })
  await t.rejects(npm.exec('patch', ['update']), { code: 'EUSAGE' })
})

// install a single version of `name` and commit a patch, then hand-edit the selector to `selectorKey`.
const installAndPatch = async (t, name, { src = 'x\n', addFile, selectorKey } = {}) => {
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': src })
  await npm.exec('install', [])
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const editDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  if (addFile) {
    fs.writeFileSync(path.join(editDir, addFile), 'extra\n')
  } else {
    fs.writeFileSync(path.join(editDir, 'index.js'), 'A\n')
  }
  await npm.exec('patch', ['commit', editDir])
  if (selectorKey) {
    const pkg = readJson(path.join(npm.prefix, 'package.json'))
    pkg.patchedDependencies = { [selectorKey]: pkg.patchedDependencies[`${name}@1.0.0`] }
    fs.writeFileSync(path.join(npm.prefix, 'package.json'), JSON.stringify(pkg))
  }
  return { npm, joinedOutput, outputs }
}

t.test('update: exact selector with no --to is a no-op', async t => {
  const { npm } = await installAndPatch(t, 'upd-noop')
  await t.rejects(npm.exec('patch', ['update', 'upd-noop']), { code: 'EPATCHNOOP' })
})

t.test('update: a name-only selector resolves the installed version', async t => {
  const { npm } = await installAndPatch(t, 'upd-nameonly', { selectorKey: 'upd-nameonly' })
  // only 1.0.0 installed, so the name-only selector resolves to it -> no-op
  await t.rejects(npm.exec('patch', ['update', 'upd-nameonly']), { code: 'EPATCHNOOP' })
})

t.test('update: a range matching no installed version rejects with EPATCHSTALE', async t => {
  const { npm } = await installAndPatch(t, 'upd-norange', { selectorKey: 'upd-norange@^5.0.0' })
  await t.rejects(npm.exec('patch', ['update', 'upd-norange']),
    { code: 'EPATCHSTALE', message: /no installed version matches the patch selector "upd-norange@\^5.0.0"/ })
})

t.test('update: a patch that no longer applies to its baseline rejects with EPATCHBASE', async t => {
  const name = 'upd-drift'
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'real\n', '2.0.0': 'real2\n' })
  await npm.exec('install', [])
  // a patch whose context does not exist in the baseline tarball cannot be re-applied during rebase
  fs.mkdirSync(path.join(npm.prefix, 'patches'), { recursive: true })
  fs.writeFileSync(path.join(npm.prefix, 'patches', `${name}@1.0.0.patch`),
    '--- a/index.js\t\n+++ b/index.js\t\n@@ -1,1 +1,1 @@\n-NOT-THE-REAL-LINE\n+changed\n')
  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  pkg.patchedDependencies = { [`${name}@1.0.0`]: `patches/${name}@1.0.0.patch` }
  fs.writeFileSync(path.join(npm.prefix, 'package.json'), JSON.stringify(pkg))
  npm.config.set('to', '2.0.0')
  await t.rejects(npm.exec('patch', ['update', name]), { code: 'EPATCHBASE' })
})

t.test('update: when the new version already contains the patch, reports EPATCHEMPTY', async t => {
  const name = 'upd-empty'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  // v2 already has the value the patch sets, so the rebase yields nothing
  await setupVersions(npm, registry, name, { '1.0.0': 'old\n', '2.0.0': 'new\n' })
  await npm.exec('install', [])
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const editDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(editDir, 'index.js'), 'new\n')
  await npm.exec('patch', ['commit', editDir])
  npm.config.set('to', '2.0.0')
  await t.rejects(npm.exec('patch', ['update', name]), { code: 'EPATCHEMPTY' })
})

t.test('update: a patches-dir outside the project is rejected', async t => {
  const name = 'upd-unsafe'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'a\nb\nc\n', '2.0.0': 'a\nb\nCC\n' })
  await npm.exec('install', [])
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const editDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(editDir, 'index.js'), 'AA\nb\nc\n')
  await npm.exec('patch', ['commit', editDir])
  npm.config.set('to', '2.0.0')
  npm.config.set('patches-dir', '../outside')
  await t.rejects(npm.exec('patch', ['update', name]), { code: 'EPATCHUNSAFE' })
})

t.test('update --to keeps a range selector when the lockfile is unknown', async t => {
  const name = 'upd-keep'
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        dependencies: { [name]: '*' },
        patchedDependencies: { [`${name}@^1.0.0`]: `patches/${name}@1.0.0.patch` },
      }),
      patches: { [`${name}@1.0.0.patch`]: '--- /dev/null\t\n+++ b/EXTRA.txt\t\n@@ -0,0 +1 @@\n+extra\n' },
    },
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'x\n', '2.0.0': 'x\n' })
  // no install -> no lockfile -> installed versions unknown; --to drives the target
  npm.config.set('to', '2.0.0')
  await npm.exec('patch', ['update', name])
  t.same(readJson(path.join(npm.prefix, 'package.json')).patchedDependencies, {
    [`${name}@^1.0.0`]: `patches/${name}@1.0.0.patch`,
    [`${name}@2.0.0`]: `patches/${name}@2.0.0.patch`,
  }, 'range kept, new exact entry added')
})

t.test('commit: a foreign update marker does not hijack a normal commit', async t => {
  const name = 'upd-foreign'
  const { npm, joinedOutput, outputs, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: rootWith({ [name]: '^1.0.0' }),
  })
  await setupVersions(npm, registry, name, { '1.0.0': 'a\n' })
  await npm.exec('install', [])
  outputs.length = 0
  await npm.exec('patch', ['add', name])
  const editDir = joinedOutput().match(/directory: (.+)/)[1].trim()
  fs.writeFileSync(path.join(editDir, 'index.js'), 'patched\n')
  // a valid marker naming a different package must be ignored, not acted on
  fs.writeFileSync(path.join(editDir, '.npm-patch-update.json'),
    JSON.stringify({ name: 'other-pkg', removeKey: 'other-pkg@9.9.9' }))
  await npm.exec('patch', ['commit', editDir])

  const pkg = readJson(path.join(npm.prefix, 'package.json'))
  t.ok(pkg.patchedDependencies[`${name}@1.0.0`], 'normal commit recorded its own selector')
  // a normal commit does a full reify, so node_modules is patched (not the metadata-only update path)
  t.equal(fs.readFileSync(path.join(npm.prefix, 'node_modules', name, 'index.js'), 'utf8'), 'patched\n',
    'node_modules is patched despite the foreign marker')
})

t.test('commit: an invalid update marker rejects with EPATCHBADMARKER', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: { 'package.json': JSON.stringify({ name: 'r', version: '1.0.0' }) },
  })
  const editDir = path.join(npm.prefix, 'ed')
  fs.mkdirSync(editDir, { recursive: true })
  fs.writeFileSync(path.join(editDir, 'package.json'), JSON.stringify({ name: 'foo', version: '1.0.0' }))
  fs.writeFileSync(path.join(editDir, '.npm-patch-update.json'), 'not json')
  await t.rejects(npm.exec('patch', ['commit', editDir]), { code: 'EPATCHBADMARKER' })
})

t.test('rm: no pkg arg rejects with EUSAGE', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: basePrefix(),
  })
  await t.rejects(npm.exec('patch', ['rm']), { code: 'EUSAGE' })
})

t.test('completion lists subcommands at the right depth', async t => {
  t.same(
    await Patch.completion({ conf: { argv: { remain: ['npm', 'patch'] } } }),
    ['add', 'commit', 'update', 'ls', 'rm']
  )
  t.same(await Patch.completion({ conf: { argv: { remain: ['npm', 'patch', 'add', 'x'] } } }), [])
})

t.test('add: ignore-existing wipes a pre-existing edit dir', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  const customDir = path.join(npm.prefix, 'reuse-edit')
  fs.mkdirSync(customDir, { recursive: true })
  fs.writeFileSync(path.join(customDir, 'stale.txt'), 'old')
  npm.config.set('edit-dir', customDir)
  npm.config.set('ignore-existing', true)
  await npm.exec('patch', ['add', DEP_NAME])
  t.notOk(fs.existsSync(path.join(customDir, 'stale.txt')), 'stale file removed')
  t.ok(fs.existsSync(path.join(customDir, 'package.json')), 'fresh extract present')
})

t.test('add: range matching an installed version resolves to it', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])
  await npm.exec('patch', ['add', `${DEP_NAME}@^${DEP_VERSION}`])
  t.match(joinedOutput(), /You can now edit the following directory: /, 'range matched the installed version')
})

t.test('add: range not installed resolves against the registry', async t => {
  const { npm, joinedOutput, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: {
      'dep-tarball': { 'package.json': JSON.stringify({ name: DEP_NAME, version: '2.0.0' }), 'index.js': DEP_SRC },
      'package.json': JSON.stringify({ name: 'root-project', version: '1.0.0' }),
    },
  })
  const manifest = registry.manifest({ name: DEP_NAME, versions: ['2.0.0'] })
  const dist = new URL(manifest.versions['2.0.0'].dist.tarball)
  const tar = await pacote.tarball(path.join(npm.prefix, 'dep-tarball'), { Arborist })
  registry.nock.get(`/${DEP_NAME}`).reply(200, manifest).persist()
  registry.nock.get(dist.pathname).reply(200, tar).persist()

  await npm.exec('patch', ['add', `${DEP_NAME}@^2.0.0`])
  t.match(joinedOutput(), /You can now edit the following directory: /, 'resolved the range via the registry')
})

t.test('commit: a patches-dir outside the project is rejected', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false, 'patches-dir': '../outside' },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  const editDir = path.join(npm.prefix, 'edit')
  await pacote.extract(`${DEP_NAME}@${DEP_VERSION}`, editDir, npm.flatOptions)
  fs.writeFileSync(path.join(editDir, 'index.js'), 'module.exports = () => "patched"\n')
  await t.rejects(
    npm.exec('patch', ['commit', editDir]),
    { code: 'EPATCHUNSAFE' },
    'commit refuses to write the patch outside the project root'
  )
})

t.test('commit: edit dir package.json missing version rejects', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify(rootPackageJson),
      'bad-edit': { 'package.json': JSON.stringify({ name: 'no-version' }) },
    },
  })
  await t.rejects(
    npm.exec('patch', ['commit', path.join(npm.prefix, 'bad-edit')]),
    /missing name or version/
  )
})

t.test('commit: keep-edit-dir leaves the edit directory in place', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false, 'keep-edit-dir': true },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  const editDir = path.join(npm.prefix, 'kept-edit')
  await pacote.extract(`${DEP_NAME}@${DEP_VERSION}`, editDir, npm.flatOptions)
  fs.writeFileSync(path.join(editDir, 'index.js'), 'module.exports = () => "patched"\n')
  await npm.exec('patch', ['commit', editDir])
  t.ok(fs.existsSync(editDir), 'edit dir kept when keep-edit-dir is set')
})

t.test('ls counts nodes for a range selector', async t => {
  // offline fixture: ls reads the installed tree from disk, no registry needed
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        ...rootPackageJson,
        patchedDependencies: { [`${DEP_NAME}@^1.0.0`]: `patches/${DEP_NAME}.patch` },
      }),
      'package-lock.json': JSON.stringify(rootPackageLock),
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
      },
    },
  })
  await npm.exec('patch', ['ls'])
  t.match(joinedOutput(), /\(1 node\)/, 'range selector matches the installed version')
})

t.test('ls tolerates ambiguous overlapping range selectors', async t => {
  // two overlapping non-subset ranges make matchSelector throw; ls must not crash
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        dependencies: { [DEP_NAME]: '1.5.0' },
        patchedDependencies: {
          [`${DEP_NAME}@>=1.0.0 <2.0.0`]: 'patches/a.patch',
          [`${DEP_NAME}@>=1.4.0 <3.0.0`]: 'patches/b.patch',
        },
      }),
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '1.5.0' }) },
      },
    },
  })
  await npm.exec('patch', ['ls'])
  t.match(joinedOutput(), /\(error: ambiguous selectors\)/, 'ls surfaces the ambiguity')
})

t.test('ls flags only the conflicting range selectors, not an exact one', async t => {
  // an exact selector for the same name must not be reported as ambiguous
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        dependencies: { [DEP_NAME]: '1.0.0', b: '1.0.0' },
        patchedDependencies: {
          [`${DEP_NAME}@1.0.0`]: 'patches/exact.patch',
          [`${DEP_NAME}@>=2.0.0 <4.0.0`]: 'patches/a.patch',
          [`${DEP_NAME}@>=3.0.0 <5.0.0`]: 'patches/b.patch',
        },
      }),
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '1.0.0' }) },
        b: {
          'package.json': JSON.stringify({ name: 'b', version: '1.0.0', dependencies: { [DEP_NAME]: '3.5.0' } }),
          node_modules: { [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '3.5.0' }) } },
        },
      },
    },
  })
  await npm.exec('patch', ['ls'])
  const out = joinedOutput()
  t.match(out, new RegExp(`patches/exact\\.patch\\t${DEP_NAME}@1\\.0\\.0\\t\\(1 node\\)`), 'exact selector counts its node')
  t.match(out, /patches\/a\.patch\t.*\(error: ambiguous selectors\)/, 'first overlapping range flagged')
  t.match(out, /patches\/b\.patch\t.*\(error: ambiguous selectors\)/, 'second overlapping range flagged')
})

t.test('ls reports plural node counts for a name-only selector', async t => {
  // offline fixture with two installed copies so the match count is plural
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        dependencies: { [DEP_NAME]: '1.0.0', b: '1.0.0' },
        patchedDependencies: { [DEP_NAME]: `patches/${DEP_NAME}.patch` },
      }),
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '1.0.0' }) },
        b: {
          'package.json': JSON.stringify({ name: 'b', version: '1.0.0', dependencies: { [DEP_NAME]: '2.0.0' } }),
          node_modules: { [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: '2.0.0' }) } },
        },
      },
    },
  })
  await npm.exec('patch', ['ls'])
  t.match(joinedOutput(), /\(2 nodes\)/, 'name-only selector matches both installed copies')
})

t.test('rm refuses to delete a patch file outside the project root', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'root-project',
        version: '1.0.0',
        patchedDependencies: { [`${DEP_NAME}@1.0.0`]: '../escape.patch' },
      }),
    },
  })
  await t.rejects(
    npm.exec('patch', ['rm', DEP_NAME]),
    { code: 'EPATCHUNSAFE' },
    'a crafted escaping patch path is not deleted'
  )
})

t.test('rm removes every selector for a bare name', async t => {
  // offline: the dep is already installed and unpatched, so rm reifies without the registry
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    prefixDir: {
      'package.json': JSON.stringify({
        ...rootPackageJson,
        patchedDependencies: {
          [`${DEP_NAME}@1.0.0`]: 'patches/one.patch',
          [`${DEP_NAME}@2.0.0`]: 'patches/two.patch',
        },
      }),
      'package-lock.json': JSON.stringify(rootPackageLock),
      patches: { 'one.patch': '', 'two.patch': '' },
      node_modules: {
        [DEP_NAME]: { 'package.json': JSON.stringify({ name: DEP_NAME, version: DEP_VERSION }) },
      },
    },
  })
  await npm.exec('patch', ['rm', DEP_NAME])
  t.match(joinedOutput(), /Removed patches:/, 'reports plural removal')
  t.notOk(readJson(path.join(npm.prefix, 'package.json')).patchedDependencies, 'all selectors removed')
})

t.test('rm keeps a patch file still referenced by another selector', async t => {
  const { npm, registry } = await loadMockNpm(t, {
    config: { 'ignore-scripts': true, audit: false },
    strictRegistryNock: false,
    prefixDir: basePrefix(),
  })
  await setupDep(npm, registry)
  await npm.exec('install', [])

  // create a real patch via the normal flow
  await npm.exec('patch', ['add', DEP_NAME])
  const editDir = path.join(npm.prefix, 'edit')
  await pacote.extract(`${DEP_NAME}@${DEP_VERSION}`, editDir, npm.flatOptions)
  fs.writeFileSync(path.join(editDir, 'index.js'), 'module.exports = () => "patched"\n')
  await npm.exec('patch', ['commit', editDir])

  // add a second name-only selector pointing at the same patch file
  const pkgPath = path.join(npm.prefix, 'package.json')
  const pkg = readJson(pkgPath)
  const patchPath = pkg.patchedDependencies[`${DEP_NAME}@${DEP_VERSION}`]
  pkg.patchedDependencies[DEP_NAME] = patchPath
  fs.writeFileSync(pkgPath, JSON.stringify(pkg))

  // removing the exact selector leaves the name-only one, so the file stays
  await npm.exec('patch', ['rm', `${DEP_NAME}@${DEP_VERSION}`])
  t.ok(fs.existsSync(path.join(npm.prefix, patchPath)), 'shared patch file retained')
  const after = readJson(pkgPath)
  t.ok(after.patchedDependencies[DEP_NAME], 'name-only selector kept')
  t.notOk(after.patchedDependencies[`${DEP_NAME}@${DEP_VERSION}`], 'exact selector removed')
})

t.test('install honors --allow-unused-patches only from the cli', async t => {
  // an empty project with a ghost patch entry triggers EPATCHUNUSED entirely offline
  const prefixDir = {
    'package.json': JSON.stringify({
      name: 'root',
      version: '1.0.0',
      patchedDependencies: { 'ghost@1.0.0': 'patches/ghost.patch' },
    }),
    patches: { 'ghost.patch': '--- a/x\n+++ b/x\n' },
  }

  t.test('unused patch is a hard error by default', async t => {
    const { npm } = await loadMockNpm(t, { config: { 'ignore-scripts': true, audit: false }, prefixDir })
    await t.rejects(npm.exec('install', []), { code: 'EPATCHUNUSED' })
  })

  t.test('the cli flag suppresses the error', async t => {
    const { npm } = await loadMockNpm(t, {
      config: { 'ignore-scripts': true, audit: false, 'allow-unused-patches': true },
      prefixDir,
    })
    await t.resolves(npm.exec('install', []))
  })

  t.test('the same flag in .npmrc is ignored', async t => {
    const { npm } = await loadMockNpm(t, {
      config: { 'ignore-scripts': true, audit: false },
      prefixDir: { ...prefixDir, '.npmrc': 'allow-unused-patches=true' },
    })
    await t.rejects(npm.exec('install', []), { code: 'EPATCHUNUSED' })
  })
})
