const t = require('tap')
const path = require('node:path')
const {
  applyApprovalForPackage,
  applyDenyForPackage,
  nameKeyFor,
  versionedKeyFor,
  isSingleVersionPin,
} = require('../../../lib/utils/allow-scripts-writer.js')

const node = (overrides = {}) => {
  const name = overrides.name ?? overrides.packageName ?? 'pkg'
  const packageName = overrides.packageName ?? name
  const version = overrides.version ?? '1.0.0'
  const urlPkg = packageName
  return {
    name,
    packageName,
    version,
    resolved: overrides.resolved
      ?? `https://registry.npmjs.org/${urlPkg}/-/${urlPkg}-${version}.tgz`,
    location: overrides.location ?? `node_modules/${name}`,
    isRegistryDependency: overrides.isRegistryDependency ?? true,
  }
}

t.test('nameKeyFor / versionedKeyFor — registry', async t => {
  const n = node({ name: 'canvas', version: '2.11.0' })
  t.equal(nameKeyFor(n), 'canvas')
  t.equal(versionedKeyFor(n), 'canvas@2.11.0')
})

t.test('nameKeyFor / versionedKeyFor — git', async t => {
  const n = node({
    name: 'bar',
    resolved: 'git+ssh://git@github.com/foo/bar.git#deadbeefcafebabe1234567890abcdef12345678',
  })
  t.equal(nameKeyFor(n), 'github:foo/bar')
  t.equal(versionedKeyFor(n), 'github:foo/bar#deadbeefcafebabe1234567890abcdef12345678')
})

t.test('nameKeyFor / versionedKeyFor — file', async t => {
  const n = node({ name: 'local', resolved: 'file:../local' })
  t.equal(nameKeyFor(n), 'file:../local')
  t.equal(versionedKeyFor(n), 'file:../local')
})

t.test('nameKeyFor / versionedKeyFor — local directory link target', async t => {
  const targetPath = path.resolve('local')
  const n = {
    name: 'local',
    packageName: 'local',
    version: '1.0.0',
    resolved: null,
    path: targetPath,
    realpath: targetPath,
    linksIn: new Set([{ resolved: 'file:../local' }]),
  }

  t.equal(nameKeyFor(n), 'file:../local')
  t.equal(versionedKeyFor(n), 'file:../local')

  t.strictSame(
    applyApprovalForPackage({}, [n], { pin: true }).allowScripts,
    { 'file:../local': true }
  )
  t.match(
    applyApprovalForPackage({ 'file:local': false }, [n], { pin: true }).warning,
    /denied|versioned deny/
  )
})

t.test('nameKeyFor / versionedKeyFor — empty link target has no portable file key', async t => {
  const targetPath = path.resolve('local')
  const n = {
    name: 'local',
    packageName: 'local',
    version: '1.0.0',
    resolved: null,
    path: targetPath,
    realpath: targetPath,
    linksIn: new Set(),
  }

  t.equal(nameKeyFor(n), null)
  t.equal(versionedKeyFor(n), null)
  t.strictSame(applyApprovalForPackage({}, [n], { pin: true }).allowScripts, {})
})

t.test('isSingleVersionPin', async t => {
  t.ok(isSingleVersionPin('pkg@1.2.3'))
  t.notOk(isSingleVersionPin('pkg'))
  t.notOk(isSingleVersionPin('pkg@^1'))
  t.notOk(isSingleVersionPin('pkg@1.2.3 || 2.0.0'))
  t.notOk(isSingleVersionPin('@@@bad'))
})

t.test('applyApprovalForPackage — empty allowScripts, --pin', async t => {
  const { allowScripts, changes } = applyApprovalForPackage(
    {},
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.strictSame(allowScripts, { 'canvas@2.11.0': true })
  t.strictSame(changes, [{ key: 'canvas@2.11.0', change: 'added' }])
})

t.test('applyApprovalForPackage — empty allowScripts, --no-pin', async t => {
  const { allowScripts, changes } = applyApprovalForPackage(
    {},
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: false }
  )
  t.strictSame(allowScripts, { canvas: true })
  t.strictSame(changes, [{ key: 'canvas', change: 'added' }])
})

t.test('applyApprovalForPackage — stale pin rewritten to new installed version', async t => {
  const { allowScripts, changes } = applyApprovalForPackage(
    { 'canvas@2.10.0': true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.strictSame(allowScripts, { 'canvas@2.11.0': true })
  t.match(changes, [
    { key: 'canvas@2.10.0', change: 'removed-stale' },
    { key: 'canvas@2.11.0', change: 'added' },
  ])
})

t.test('applyApprovalForPackage — multi-version disjunction is preserved', async t => {
  const { allowScripts } = applyApprovalForPackage(
    { 'canvas@2.10.0 || 2.11.0': true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.strictSame(allowScripts, {
    'canvas@2.10.0 || 2.11.0': true,
    'canvas@2.11.0': true,
  })
})

t.test('applyApprovalForPackage — already-allowed exact version is a no-op', async t => {
  const { allowScripts, changes } = applyApprovalForPackage(
    { 'canvas@2.11.0': true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.strictSame(allowScripts, { 'canvas@2.11.0': true })
  t.strictSame(changes, [])
})

t.test('applyApprovalForPackage — existing deny wins, returns warning', async t => {
  const { allowScripts, changes, warning } = applyApprovalForPackage(
    { canvas: false },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.strictSame(allowScripts, { canvas: false })
  t.strictSame(changes, [])
  t.match(warning, /canvas is denied/)
})

t.test('applyApprovalForPackage — versioned deny wins too', async t => {
  const { changes, warning } = applyApprovalForPackage(
    { 'canvas@2.11.0': false },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.strictSame(changes, [])
  t.match(warning, /denied|versioned deny/)
})

t.test('applyApprovalForPackage — name-only existing, --no-pin no-op', async t => {
  const { allowScripts, changes } = applyApprovalForPackage(
    { canvas: true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: false }
  )
  t.strictSame(allowScripts, { canvas: true })
  t.strictSame(changes, [])
})

t.test('applyApprovalForPackage — --no-pin downgrades pinned entry to name-only', async t => {
  const { allowScripts } = applyApprovalForPackage(
    { 'canvas@2.10.0': true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: false }
  )
  t.strictSame(allowScripts, { canvas: true })
})

t.test('applyApprovalForPackage — multiple installed versions write multiple pins', async t => {
  const { allowScripts } = applyApprovalForPackage(
    {},
    [
      node({ name: 'lodash', version: '4.17.21' }),
      node({ name: 'lodash', version: '3.10.1' }),
    ],
    { pin: true }
  )
  t.strictSame(allowScripts, { 'lodash@3.10.1': true, 'lodash@4.17.21': true })
})

t.test('applyApprovalForPackage — keeps existing pin matching one installed, adds pin for other', async t => {
  const { allowScripts } = applyApprovalForPackage(
    { 'lodash@4.17.21': true },
    [
      node({ name: 'lodash', version: '4.17.21' }),
      node({ name: 'lodash', version: '3.10.1' }),
    ],
    { pin: true }
  )
  t.strictSame(allowScripts, { 'lodash@3.10.1': true, 'lodash@4.17.21': true })
})

t.test('applyDenyForPackage — empty allowScripts adds name-only false', async t => {
  const { allowScripts, changes } = applyDenyForPackage(
    {},
    [node({ name: 'core-js', version: '3.0.0' })]
  )
  t.strictSame(allowScripts, { 'core-js': false })
  t.strictSame(changes, [{ key: 'core-js', change: 'added' }])
})

t.test('applyDenyForPackage — pinned allow is replaced by name-only deny', async t => {
  const { allowScripts } = applyDenyForPackage(
    { 'core-js@3.0.0': true },
    [node({ name: 'core-js', version: '3.0.0' })]
  )
  t.strictSame(allowScripts, { 'core-js': false })
})

t.test('applyDenyForPackage — already-denied is a no-op', async t => {
  const { changes } = applyDenyForPackage(
    { 'core-js': false },
    [node({ name: 'core-js', version: '3.0.0' })]
  )
  t.strictSame(changes, [])
})

t.test('applyDenyForPackage — name-only true is replaced by name-only false', async t => {
  const { allowScripts } = applyDenyForPackage(
    { 'core-js': true },
    [node({ name: 'core-js', version: '3.0.0' })]
  )
  t.strictSame(allowScripts, { 'core-js': false })
})

t.test('applyApprovalForPackage — preserves unrelated entries', async t => {
  const { allowScripts } = applyApprovalForPackage(
    { other: true, 'unrelated@1.0.0': false },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.strictSame(allowScripts, {
    other: true,
    'unrelated@1.0.0': false,
    'canvas@2.11.0': true,
  })
})

t.test('applyApprovalForPackage — git node writes hosted shortcut with commit', async t => {
  const { allowScripts } = applyApprovalForPackage(
    {},
    [node({
      name: 'bar',
      resolved: 'git+ssh://git@github.com/foo/bar.git#deadbeefcafebabe1234567890abcdef12345678',
    })],
    { pin: true }
  )
  t.strictSame(allowScripts, {
    'github:foo/bar#deadbeefcafebabe1234567890abcdef12345678': true,
  })
})

t.test('applyApprovalForPackage — git node --no-pin writes hosted shortcut without commit', async t => {
  const { allowScripts } = applyApprovalForPackage(
    {},
    [node({
      name: 'bar',
      resolved: 'git+ssh://git@github.com/foo/bar.git#deadbeef',
    })],
    { pin: false }
  )
  t.strictSame(allowScripts, { 'github:foo/bar': true })
})

t.test('applyApprovalForPackage — file dep uses resolved as both keys', async t => {
  const { allowScripts } = applyApprovalForPackage(
    {},
    [node({ name: 'local', resolved: 'file:../local' })],
    { pin: true }
  )
  t.strictSame(allowScripts, { 'file:../local': true })
})

t.test('applyApprovalForPackage — empty nodes returns unchanged', async t => {
  const { allowScripts, changes } = applyApprovalForPackage({ x: true }, [], { pin: true })
  t.strictSame(allowScripts, { x: true })
  t.strictSame(changes, [])
})

t.test('applyApprovalForPackage — name-only entry is replaced by pin (RFC table)', async t => {
  const { allowScripts, changes } = applyApprovalForPackage(
    { canvas: true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  // Per RFC table: pkg: true + --pin must upgrade to pkg@x.y.z: true.
  // Both entries left behind would be wrong.
  t.strictSame(allowScripts, { 'canvas@2.11.0': true })
  t.match(changes, [
    { key: 'canvas@2.11.0', change: 'added' },
    { key: 'canvas', change: 'replaced-by-pin' },
  ])
})

t.test('applyApprovalForPackage — name-only + multi-version installs replaces with all pins', async t => {
  const { allowScripts } = applyApprovalForPackage(
    { lodash: true },
    [
      node({ name: 'lodash', version: '4.17.21' }),
      node({ name: 'lodash', version: '3.10.1' }),
    ],
    { pin: true }
  )
  t.strictSame(allowScripts, { 'lodash@3.10.1': true, 'lodash@4.17.21': true })
})

t.test('applyApprovalForPackage — name-only is preserved when --no-pin', async t => {
  const { allowScripts, changes } = applyApprovalForPackage(
    { canvas: true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: false }
  )
  t.strictSame(allowScripts, { canvas: true })
  t.strictSame(changes, [])
})

t.test('applyApprovalForPackage — name-only NOT dropped when no pinning could happen', async t => {
  // Node has no version, so installedKeys is empty. The name-only entry
  // must NOT be dropped or we silently lose the policy.
  const noVersion = { name: 'pkg', packageName: 'pkg', version: undefined, resolved: 'https://registry.npmjs.org/pkg/-/pkg-1.tgz' }
  const { allowScripts } = applyApprovalForPackage(
    { pkg: true },
    [noVersion],
    { pin: true }
  )
  t.strictSame(allowScripts, { pkg: true })
})

t.test('applyApprovalForPackage — convergent: running twice gives the same result', async t => {
  // Start with stale state including a name-only entry.
  const start = { canvas: true, 'canvas@2.10.0': true }
  const nodes = [node({ name: 'canvas', version: '2.11.0' })]

  const run1 = applyApprovalForPackage(start, nodes, { pin: true })
  const run2 = applyApprovalForPackage(run1.allowScripts, nodes, { pin: true })

  t.strictSame(run1.allowScripts, { 'canvas@2.11.0': true })
  t.strictSame(run2.allowScripts, { 'canvas@2.11.0': true })
  t.strictSame(run2.changes, [], 'second run is a no-op')
})

t.test('applyApprovalForPackage — deny still wins even when name-only is upgraded', async t => {
  const { allowScripts, warning } = applyApprovalForPackage(
    { canvas: true, 'canvas@2.11.0': false },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  // Existing deny on the version blocks the approval.
  t.strictSame(allowScripts, { canvas: true, 'canvas@2.11.0': false })
  t.match(warning, /denied|versioned deny/)
})

t.test('keyTargetsNode — unparseable key returns false (via applyApproval)', async t => {
  // An unparseable key in the existing object should be ignored.
  const { allowScripts } = applyApprovalForPackage(
    { '@@@invalid': true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.equal(allowScripts['canvas@2.11.0'], true)
  t.equal(allowScripts['@@@invalid'], true)
})

t.test('applyDenyForPackage — empty nodes array returns unchanged', async t => {
  const { allowScripts, changes } = applyDenyForPackage({ existing: true }, [])
  t.strictSame(allowScripts, { existing: true })
  t.strictSame(changes, [])
})

t.test('applyDenyForPackage — node with no nameable identity is a no-op', async t => {
  // A node whose resolved field is unparseable as a git URL and has no
  // version/name produces a null name; the writer must short-circuit.
  const weird = { name: '', packageName: '', version: undefined, resolved: undefined }
  const { allowScripts, changes } = applyDenyForPackage({}, [weird])
  t.strictSame(allowScripts, {})
  t.strictSame(changes, [])
})

t.test('applyApprovalForPackage — file dep with deny entry blocks approval', async t => {
  const { warning } = applyApprovalForPackage(
    { 'file:../local': false },
    [node({ name: 'local', resolved: 'file:../local' })],
    { pin: true }
  )
  t.match(warning, /denied|versioned deny/)
})

t.test('applyApprovalForPackage — remote tarball deny blocks approval', async t => {
  const remote = { name: 'pkg', packageName: 'pkg', version: '1.0.0', resolved: 'https://example.com/pkg.tgz' }
  const { warning } = applyApprovalForPackage(
    { 'https://example.com/pkg.tgz': false },
    [remote],
    { pin: true }
  )
  t.match(warning, /denied|versioned deny/)
})

t.test('applyApprovalForPackage — no-pin with no name produces no-op', async t => {
  const weird = { name: '', packageName: '', resolved: 'git+ssh://no.parse' }
  const { allowScripts, changes } = applyApprovalForPackage({}, [weird], { pin: false })
  t.strictSame(allowScripts, {})
  t.strictSame(changes, [])
})

t.test('applyApprovalForPackage — pin with no versioned key is a no-op', async t => {
  const noVersion = { name: 'pkg', packageName: 'pkg', version: undefined, resolved: 'https://registry.npmjs.org/pkg/-/pkg-1.tgz' }
  const { allowScripts, changes } = applyApprovalForPackage({}, [noVersion], { pin: true })
  t.strictSame(allowScripts, {})
  t.strictSame(changes, [])
})

t.test('applyApprovalForPackage — pin with no versioned key and existing name-only is no-op', async t => {
  const noVersion = { name: 'pkg', packageName: 'pkg', version: undefined, resolved: 'https://registry.npmjs.org/pkg/-/pkg-1.tgz' }
  const { changes } = applyApprovalForPackage({ pkg: true }, [noVersion], { pin: true })
  t.strictSame(changes, [])
})

t.test('keyTargetsNode handles file with directory-typed key', async t => {
  // A "directory" spec for a relative path.
  const dirNode = { name: 'local', packageName: 'local', resolved: 'file:./local-dir' }
  const { allowScripts } = applyApprovalForPackage(
    {},
    [dirNode],
    { pin: true }
  )
  t.equal(allowScripts['file:./local-dir'], true)
})

t.test('nameKeyFor / versionedKeyFor — null node', async t => {
  t.equal(nameKeyFor(null), null)
  t.equal(versionedKeyFor(null), null)
})

t.test('nameKeyFor / versionedKeyFor — non-hosted git url returns null', async t => {
  const n = { name: 'pkg', packageName: 'pkg', resolved: 'git+https://example.invalid/foo/bar.git#abc' }
  t.equal(nameKeyFor(n), null)
  t.equal(versionedKeyFor(n), null)
})

t.test('versionedKeyFor — absolute path resolved field', async t => {
  const n = { name: 'pkg', packageName: 'pkg', resolved: '/abs/path/local' }
  t.equal(versionedKeyFor(n), '/abs/path/local')
  t.equal(nameKeyFor(n), '/abs/path/local')
})

t.test('applyApprovalForPackage — node.resolved parse error in keyTargetsNode is safe', async t => {
  // An existing git-style key for a package whose own resolved field
  // doesn't parse: the key just doesn't target anything.
  const gitNode = node({
    name: 'bar',
    resolved: 'git+ssh://git@github.com/foo/bar.git#abc',
  })
  // Add an explicit unparseable existing entry.
  const { allowScripts } = applyApprovalForPackage(
    { 'github:other/other': true },
    [gitNode],
    { pin: true }
  )
  // Existing entry unchanged; new git entry added.
  t.equal(allowScripts['github:other/other'], true)
  t.equal(allowScripts['github:foo/bar#abc'], true)
})

t.test('keyTargetsNode — alias key does not target anything (via writer)', async t => {
  // Alias-typed key falls through the switch default.
  const { allowScripts } = applyApprovalForPackage(
    { 'foo@npm:bar@1.0.0': true },
    [node({ name: 'foo', packageName: 'foo', version: '1.0.0' })],
    { pin: true }
  )
  // Alias entry untouched, new pin added separately.
  t.equal(allowScripts['foo@npm:bar@1.0.0'], true)
  t.equal(allowScripts['foo@1.0.0'], true)
})
t.test('keyTargetsNode handles tag-type key', async t => {
  // 'canvas@latest' parses as type='tag'. The writer should treat it like
  // a name-only match (any installed version of canvas).
  const { allowScripts } = applyApprovalForPackage(
    { 'canvas@latest': true },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  // The tag key targets the canvas node (same package name), so the
  // 'canvas@2.11.0' pin gets added; tag key is preserved.
  t.equal(allowScripts['canvas@latest'], true)
  t.equal(allowScripts['canvas@2.11.0'], true)
})

t.test('keyTargetsNode handles file-type tarball key matching saveSpec', async t => {
  // 'file:pkg.tgz' parses as type='file' with saveSpec='file:pkg.tgz'.
  const tarballNode = {
    name: 'pkg',
    packageName: 'pkg',
    version: '1.0.0',
    resolved: 'file:pkg.tgz',
  }
  const { allowScripts } = applyApprovalForPackage(
    { 'file:pkg.tgz': false },
    [tarballNode],
    { pin: true }
  )
  // saveSpec match: deny wins, no pin added.
  t.equal(allowScripts['file:pkg.tgz'], false)
})

t.test('keyTargetsNode handles file-type tarball key matching fetchSpec', async t => {
  // When node.resolved is an absolute path matching parsed.fetchSpec.
  // Use path.resolve so the absolute path is platform-correct (npa
  // parses POSIX-style `/abs/...` as a directory on Windows).
  const absTgz = path.resolve('pkg.tgz')
  const tarballNode = {
    name: 'pkg',
    packageName: 'pkg',
    version: '1.0.0',
    resolved: absTgz,
  }
  const { allowScripts, warning } = applyApprovalForPackage(
    { './pkg.tgz': false },
    [tarballNode],
    { pin: true }
  )
  t.equal(allowScripts['./pkg.tgz'], false)
  t.match(warning, /denied|versioned deny/)
})

t.test('versionedKeyFor — git node without committish', async t => {
  // versionedKeyFor's ternary takes the "no committish" branch.
  t.equal(
    versionedKeyFor({
      name: 'bar',
      resolved: 'git+ssh://git@github.com/foo/bar.git',
    }),
    'github:foo/bar'
  )
})

t.test('versionedKeyFor / nameKeyFor — absolute path resolved field', async t => {
  // Hits the `resolved.startsWith('/')` branch in both helpers.
  const n = { name: 'pkg', packageName: 'pkg', resolved: '/abs/local-dir' }
  t.equal(versionedKeyFor(n), '/abs/local-dir')
  t.equal(nameKeyFor(n), '/abs/local-dir')
})

t.test('keyTargetsNode — git key against a node with no resolved field', async t => {
  // Defensive: if existing has a git-shaped key and the installed node
  // has no resolved field, keyTargetsNode bails out and no policy entry
  // can be derived from untrusted sources.
  const noResolved = { name: 'bar', packageName: 'bar', resolved: undefined }
  const { allowScripts } = applyApprovalForPackage(
    { 'github:foo/bar': true },
    [noResolved],
    { pin: false }
  )
  // Existing entry untouched. No new key written: nameKeyFor returns
  // null for a node with no trusted identity source.
  t.equal(allowScripts['github:foo/bar'], true)
  t.notOk('bar' in allowScripts, 'no entry written under attacker-controlled node.name')
})

t.test('applyApprovalForPackage — default args (no options object)', async t => {
  // Hits the `{ pin = true } = {}` default-arg branch.
  const { allowScripts } = applyApprovalForPackage(
    {},
    [node({ name: 'canvas', version: '2.11.0' })]
  )
  t.strictSame(allowScripts, { 'canvas@2.11.0': true })
})

t.test('applyApprovalForPackage — deny-wins warning when node has no name', async t => {
  // Hits the `name || 'this package'` fallback in the warning message.
  const noName = { name: '', packageName: '', resolved: 'git+ssh://no.parse' }
  const { warning } = applyApprovalForPackage(
    { 'github:foo/bar': false },
    [noName],
    { pin: true }
  )
  // No keys target this node (its resolved doesn't parse to a hosted URL),
  // so deny-wins doesn't trigger. Result is no warning.
  t.notOk(warning)
})

t.test('denyWarning branches on key shape per RFC §approve-scripts', async t => {
  // Name-only deny: only remedy is to remove the entry.
  const nameOnly = applyApprovalForPackage(
    { canvas: false },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.match(nameOnly.warning, /remove the entry from allowScripts/)
  t.notMatch(nameOnly.warning, /widen the deny/)

  // Pinned deny on a different version: suggest both widen and remove.
  const pinned = applyApprovalForPackage(
    { 'canvas@2.10.0': false },
    [node({ name: 'canvas', version: '2.10.0' })],
    { pin: true }
  )
  t.match(pinned.warning, /versioned deny/)
  t.match(pinned.warning, /npm deny-scripts canvas/)
  t.match(pinned.warning, /widen the deny to all versions/)
  t.match(pinned.warning, /remove the entry/)

  // Multi-version deny disjunction: same as pinned (versioned).
  const multi = applyApprovalForPackage(
    { 'canvas@2.10.0 || 2.11.0': false },
    [node({ name: 'canvas', version: '2.10.0' })],
    { pin: true }
  )
  t.match(multi.warning, /versioned deny/)
  t.match(multi.warning, /npm deny-scripts canvas/)
})

t.test('denyWarning: tag-type key (pkg@latest: false) is name-only', async t => {
  // `canvas@latest` parses as type='tag'. Treat the same as a bare name.
  const { warning } = applyApprovalForPackage(
    { 'canvas@latest': false },
    [node({ name: 'canvas', version: '2.11.0' })],
    { pin: true }
  )
  t.match(warning, /remove the entry/)
  t.notMatch(warning, /versioned deny/)
})

t.test('applyApprovalForPackage — multi-version entry + --pin=false adds name-only alongside', async t => {
  // RFC table: existing `pkg@a.b.c || d.e.f: true` + installed `pkg@x.y.z`
  // + --pin=false adds `pkg: true`. The multi-version disjunction stays
  // (it captures intent the command can't infer), and the name-only
  // entry is added.
  const { allowScripts } = applyApprovalForPackage(
    { 'canvas@1.0.0 || 2.0.0': true },
    [node({ name: 'canvas', version: '3.0.0' })],
    { pin: false }
  )
  t.strictSame(allowScripts, {
    'canvas@1.0.0 || 2.0.0': true,
    canvas: true,
  })
})

t.test('versionedKeyFor — registry resolved that versionFromTgz cannot parse returns null', async t => {
  // Private-registry mirror / alternate CDN URL shape that doesn't match
  // the standard `/-/name-version.tgz` pattern. Exercises the log.silly
  // breadcrumb path in versionedKeyFor, including each fallback branch
  // of the `node.path || node.name || '<unknown>'` label expression.
  const resolved = 'https://private-mirror.example.com/blobs/abc123'
  t.equal(versionedKeyFor({
    path: '/fake/mystery', name: 'mystery', resolved, isRegistryDependency: true,
  }), null, 'falls back when node has a path')
  t.equal(versionedKeyFor({
    name: 'mystery', resolved, isRegistryDependency: true,
  }), null, 'falls back when node has only a name')
  t.equal(versionedKeyFor({
    resolved, isRegistryDependency: true,
  }), null, 'falls back when node has neither path nor name')
})
