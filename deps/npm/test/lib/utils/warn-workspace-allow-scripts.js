const t = require('tap')
const {
  findWorkspaceAllowScripts,
  warnWorkspaceAllowScripts,
} = require('../../../lib/utils/warn-workspace-allow-scripts.js')

const node = ({
  name = 'pkg',
  packageName,
  isWorkspace = false,
  isProjectRoot = false,
  allowScripts,
  path = `/fake/${name}`,
} = {}) => ({
  name,
  packageName: packageName ?? name,
  path,
  isWorkspace,
  isProjectRoot,
  package: allowScripts !== undefined ? { allowScripts } : {},
})

const tree = (nodes) => ({
  inventory: new Map(nodes.map((n, i) => [`node_modules/${n.name || `n${i}`}`, n])),
})

t.test('returns [] for empty tree', async t => {
  t.strictSame(findWorkspaceAllowScripts(tree([])), [])
})

t.test('returns [] for missing tree', async t => {
  t.strictSame(findWorkspaceAllowScripts(null), [])
  t.strictSame(findWorkspaceAllowScripts(undefined), [])
})

t.test('ignores project root with allowScripts', async t => {
  const t1 = tree([
    node({ name: 'root', isProjectRoot: true, isWorkspace: true, allowScripts: { x: true } }),
  ])
  t.strictSame(findWorkspaceAllowScripts(t1), [])
})

t.test('ignores non-workspace dep with allowScripts', async t => {
  const t1 = tree([
    node({ name: 'dep', allowScripts: { x: true } }),
  ])
  t.strictSame(findWorkspaceAllowScripts(t1), [])
})

t.test('finds non-root workspace with allowScripts', async t => {
  const ws = node({ name: 'ws', isWorkspace: true, allowScripts: { x: true } })
  const t1 = tree([
    node({ name: 'root', isProjectRoot: true, isWorkspace: true }),
    ws,
  ])
  t.equal(findWorkspaceAllowScripts(t1).length, 1)
  t.equal(findWorkspaceAllowScripts(t1)[0], ws)
})

t.test('finds workspace with empty allowScripts object too', async t => {
  const ws = node({ name: 'ws', isWorkspace: true, allowScripts: {} })
  t.equal(findWorkspaceAllowScripts(tree([ws])).length, 1)
})

t.test('warnWorkspaceAllowScripts emits one log.warn per offender', async t => {
  const warnings = []
  const listener = (level, ...args) => {
    if (level === 'warn') {
      warnings.push(args)
    }
  }
  process.on('log', listener)
  t.teardown(() => process.off('log', listener))

  const t1 = tree([
    node({ name: 'root', isProjectRoot: true, isWorkspace: true }),
    node({ name: 'a', isWorkspace: true, allowScripts: { x: true } }),
    node({ name: 'b', isWorkspace: true, allowScripts: { y: false } }),
    node({ name: 'c', isWorkspace: true }), // no allowScripts; no warning
  ])
  warnWorkspaceAllowScripts(t1)

  t.equal(warnings.length, 2)
  t.match(warnings[0][1], /allowScripts in workspace a/)
  t.match(warnings[1][1], /allowScripts in workspace b/)
})

t.test('warnWorkspaceAllowScripts uses node.name when packageName missing', async t => {
  const warnings = []
  const listener = (level, ...args) => {
    if (level === 'warn') {
      warnings.push(args)
    }
  }
  process.on('log', listener)
  t.teardown(() => process.off('log', listener))

  // packageName undefined, name set
  const ws = {
    name: 'fallback-name',
    path: '/x',
    isWorkspace: true,
    isProjectRoot: false,
    package: { allowScripts: { x: true } },
  }
  warnWorkspaceAllowScripts({ inventory: new Map([['node_modules/ws', ws]]) })
  t.match(warnings[0][1], /workspace fallback-name/)
})
