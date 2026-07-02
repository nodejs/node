const t = require('tap')
const validateLockfile = require('../../../lib/utils/validate-lockfile.js')
const { validatePackageExtensions } = require('../../../lib/utils/validate-lockfile.js')

// build mock virtual/ideal trees for validatePackageExtensions
const tree = ({ hash = null, packageExtensions, nodes = [] }) => ({
  meta: { packageExtensionsHash: hash },
  target: { package: packageExtensions === undefined ? {} : { packageExtensions } },
  inventory: { values: () => nodes },
})

t.test('packageExtensions: matching hashes and clean tree', async t => {
  const errors = validatePackageExtensions(
    tree({ hash: 'sha512-abc' }),
    tree({ hash: 'sha512-abc' })
  )
  t.strictSame(errors, [], 'no errors when hashes match and no provenance')
})

t.test('packageExtensions: both absent', async t => {
  t.strictSame(validatePackageExtensions(tree({}), tree({})), [], 'no errors when neither has state')
})

t.test('packageExtensions: missing from lock file', async t => {
  const errors = validatePackageExtensions(tree({ hash: null }), tree({ hash: 'sha512-abc' }))
  t.match(errors[0], /Missing: packageExtensions state from lock file/, 'reports missing lock state')
})

t.test('packageExtensions: present in lock but not package.json', async t => {
  const errors = validatePackageExtensions(tree({ hash: 'sha512-abc' }), tree({ hash: null }))
  t.match(errors[0], /lock file records packageExtensions state but package.json has none/, 'reports stray lock state')
})

t.test('packageExtensions: hash mismatch', async t => {
  const errors = validatePackageExtensions(tree({ hash: 'sha512-aaa' }), tree({ hash: 'sha512-bbb' }))
  t.match(errors[0], /do not match the lock file/, 'reports a mismatch')
})

t.test('packageExtensions: stale provenance with matching hash', async t => {
  // both hashes equal, but a locked node references a selector that no longer exists
  const node = { name: 'foo', version: '1.0.0', packageExtensionsApplied: { selector: 'foo@1', dependencies: ['bar'] } }
  const errors = validatePackageExtensions(
    tree({ hash: 'h', nodes: [node] }),
    tree({ hash: 'h', packageExtensions: {} })
  )
  t.match(errors[0], /stale packageExtensions provenance for foo@1.0.0/, 'reports stale provenance')
})

t.test('packageExtensions: valid provenance with matching hash', async t => {
  const node = { name: 'foo', version: '1.0.0', packageExtensionsApplied: { selector: 'foo@1', dependencies: ['bar'] } }
  // root and workspace nodes are skipped by the validation
  const root = { name: 'root', version: '1.0.0', isProjectRoot: true }
  const errors = validatePackageExtensions(
    tree({ hash: 'h', nodes: [root, node] }),
    tree({ hash: 'h', packageExtensions: { 'foo@1': { dependencies: { bar: '^1' } } } })
  )
  t.strictSame(errors, [], 'no errors when provenance still matches a selector')
})

t.test('packageExtensions: ideal tree without a target uses the tree itself', async t => {
  const idealTree = {
    meta: { packageExtensionsHash: 'h' },
    package: { packageExtensions: { 'foo@1': { dependencies: { bar: '^1' } } } },
    inventory: { values: () => [] },
  }
  t.strictSame(validatePackageExtensions(tree({ hash: 'h' }), idealTree), [], 'reads package off the tree directly')
})

t.test('packageExtensions: invalid rule set surfaces the engine error', async t => {
  const errors = validatePackageExtensions(
    tree({ hash: 'h' }),
    tree({ hash: 'h', packageExtensions: { foo: { devDependencies: { a: '1' } } } })
  )
  t.match(errors[0], /Invalid: .*unsupported field/, 'reports the engine validation error')
})

t.test('packageExtensions: alias node matches the underlying package name', async t => {
  // an aliased install: node.name is the alias, node.packageName is the real package
  const node = {
    name: 'my-alias',
    packageName: 'real-pkg',
    version: '1.0.0',
    packageExtensionsApplied: { selector: 'real-pkg@1', dependencies: ['bar'] },
  }
  const errors = validatePackageExtensions(
    tree({ hash: 'h', nodes: [node] }),
    tree({ hash: 'h', packageExtensions: { 'real-pkg@1': { dependencies: { bar: '^1' } } } })
  )
  t.strictSame(errors, [], 'provenance validated against the underlying package name, not the alias')
})

t.test('packageExtensions: locked identity matching two selectors', async t => {
  const node = { name: 'foo', version: '1.0.0' }
  const errors = validatePackageExtensions(
    tree({ hash: 'h', nodes: [node] }),
    tree({ hash: 'h', packageExtensions: { foo: { dependencies: { a: '^1' } }, 'foo@1': { dependencies: { b: '^1' } } } })
  )
  t.match(errors[0], /Multiple packageExtensions selectors match foo@1.0.0/, 'reports a selector conflict')
})

t.test('identical inventory for both idealTree and virtualTree', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ])
    ),
    'should have no errors on identical inventories'
  )
})

t.test('extra inventory items on idealTree', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
        ['baz', { name: 'baz', version: '3.0.0' }],
      ])
    ),
    'should have missing entries error'
  )
})

t.test('extra inventory items on virtualTree', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
        ['baz', { name: 'baz', version: '3.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ])
    ),
    'should have no errors if finding virtualTree extra items'
  )
})

t.test('mismatching versions on inventory', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '2.0.0' }],
        ['bar', { name: 'bar', version: '3.0.0' }],
      ])
    ),
    'should have errors for each mismatching version'
  )
})

t.test('mismatching patch integrity or path', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0', patched: { path: 'patches/foo.patch', integrity: 'sha512-aaa' } }],
        ['bar', { name: 'bar', version: '2.0.0', patched: { path: 'patches/bar.patch', integrity: 'sha512-bbb' } }],
        ['baz', { name: 'baz', version: '3.0.0' }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0', patched: { path: 'patches/foo.patch', integrity: 'sha512-CHANGED' } }],
        ['bar', { name: 'bar', version: '2.0.0', patched: { path: 'patches/moved.patch', integrity: 'sha512-bbb' } }],
        ['baz', { name: 'baz', version: '3.0.0', patched: { path: 'patches/baz.patch', integrity: 'sha512-ccc' } }],
      ])
    ),
    'should error on integrity drift, path drift, and a newly added patch'
  )
})

t.test('lock file records a patch package.json no longer declares', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([
        ['foo', { name: 'foo', version: '1.0.0', patched: { path: 'patches/foo.patch', integrity: 'sha512-aaa' } }],
      ]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
      ])
    ),
    'should report a stray lock file patch'
  )
})

t.test('missing virtualTree inventory', async t => {
  t.matchSnapshot(
    validateLockfile(
      new Map([]),
      new Map([
        ['foo', { name: 'foo', version: '1.0.0' }],
        ['bar', { name: 'bar', version: '2.0.0' }],
        ['baz', { name: 'baz', version: '3.0.0' }],
      ])
    ),
    'should have errors for each mismatching version'
  )
})
