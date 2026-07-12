const t = require('tap')

const mockCheck = (t, mocks = {}) =>
  t.mock('../../../lib/utils/check-allow-scripts.js', mocks)

// Build a minimal "arborist tree" fixture for the walker.
const arb = ({ nodes, allowScripts = null, ignoreScripts = false } = {}) => ({
  options: { allowScripts, ignoreScripts },
  actualTree: {
    inventory: new Map(nodes.map((n, i) => [`node_modules/${n.name || `n${i}`}`, n])),
  },
})

const node = ({
  name = 'pkg',
  packageName,
  version = '1.0.0',
  resolved,
  scripts = {},
  gypfile,
  path: nodePath = `/fake/${name}`,
  isProjectRoot = false,
  isWorkspace = false,
  isLink = false,
  isRegistryDependency,
} = {}) => {
  const pkgName = packageName ?? name
  const resolvedUrl = resolved
    ?? `https://registry.npmjs.org/${pkgName}/-/${pkgName}-${version}.tgz`
  // Default isRegistryDependency to match the shape of resolved: registry
  // tarballs are registry, anything else (git, file, remote) is not.
  const isReg = isRegistryDependency ?? /^https?:\/\/[^/]+\/.+\/-\/[^/]+-\d/.test(resolvedUrl)
  return {
    name,
    packageName: pkgName,
    version,
    resolved: resolvedUrl,
    location: `node_modules/${name}`,
    isRegistryDependency: isReg,
    path: nodePath,
    isProjectRoot,
    isWorkspace,
    isLink,
    package: { scripts, ...(gypfile !== undefined ? { gypfile } : {}) },
  }
}

t.test('returns [] when ignoreScripts is set', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [node({ scripts: { install: 'do-stuff' } })],
      ignoreScripts: true,
    }),
    npm: { flatOptions: {} },
  })
  t.strictSame(result, [])
})

t.test('returns unreviewed nodes when ignoreScripts is set but includeWhenIgnored is true', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [node({ name: 'a', scripts: { install: 'do-stuff' } })],
      ignoreScripts: true,
    }),
    npm: { flatOptions: {} },
    includeWhenIgnored: true,
  })
  t.equal(result.length, 1)
  t.strictSame(result[0].scripts, { install: 'do-stuff' })
})

t.test('returns [] when dangerouslyAllowAllScripts is set', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({ nodes: [node({ scripts: { install: 'do-stuff' } })] }),
    npm: { flatOptions: { dangerouslyAllowAllScripts: true } },
  })
  t.strictSame(result, [])
})

t.test('skips project root, workspace, and linked nodes', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [
        node({ name: 'root', scripts: { install: 'x' }, isProjectRoot: true }),
        node({ name: 'ws', scripts: { install: 'x' }, isWorkspace: true }),
        node({ name: 'linked', scripts: { install: 'x' }, isLink: true }),
      ],
    }),
    npm: { flatOptions: {} },
  })
  t.strictSame(result, [])
})

t.test('skips nodes with no install-relevant scripts', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [node({ scripts: { test: 'jest' } })],
    }),
    npm: { flatOptions: {} },
  })
  t.strictSame(result, [])
})

t.test('includes nodes with preinstall/install/postinstall', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [
        node({ name: 'a', scripts: { preinstall: 'pre' } }),
        node({ name: 'b', scripts: { install: 'inst' } }),
        node({ name: 'c', scripts: { postinstall: 'post' } }),
      ],
    }),
    npm: { flatOptions: {} },
  })
  t.equal(result.length, 3)
  t.strictSame(result[0].scripts, { preinstall: 'pre' })
  t.strictSame(result[1].scripts, { install: 'inst' })
  t.strictSame(result[2].scripts, { postinstall: 'post' })
})

t.test('prepare counts for non-registry sources only', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [
        // registry: prepare ignored
        node({
          name: 'registry-pkg',
          resolved: 'https://registry.npmjs.org/registry-pkg/-/registry-pkg-1.0.0.tgz',
          scripts: { prepare: 'do' },
        }),
        // git: prepare counts
        node({
          name: 'git-pkg',
          resolved: 'git+ssh://git@github.com/foo/bar.git#abcdef0123456789',
          scripts: { prepare: 'do' },
        }),
      ],
    }),
    npm: { flatOptions: {} },
  })
  t.equal(result.length, 1)
  t.equal(result[0].node.name, 'git-pkg')
})

t.test('skips approved nodes', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [node({ name: 'allowed', scripts: { install: 'x' } })],
      allowScripts: { allowed: true },
    }),
    npm: { flatOptions: {} },
  })
  t.strictSame(result, [])
})

t.test('skips denied nodes (false counts as reviewed)', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [node({ name: 'denied', scripts: { install: 'x' } })],
      allowScripts: { denied: false },
    }),
    npm: { flatOptions: {} },
  })
  t.strictSame(result, [])
})

t.test('includes unreviewed nodes when policy is set but does not cover them', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [
        node({ name: 'allowed', scripts: { install: 'x' } }),
        node({ name: 'unreviewed', scripts: { install: 'y' } }),
      ],
      allowScripts: { allowed: true },
    }),
    npm: { flatOptions: {} },
  })
  t.equal(result.length, 1)
  t.equal(result[0].node.name, 'unreviewed')
})

t.test('reports every install-script node when no policy is set', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: arb({
      nodes: [
        node({ name: 'a', scripts: { install: 'x' } }),
        node({ name: 'b', scripts: { postinstall: 'y' } }),
      ],
    }),
    npm: { flatOptions: {} },
  })
  t.equal(result.length, 2)
})

t.test('survives missing actualTree', async t => {
  const checkAllowScripts = mockCheck(t)
  const result = await checkAllowScripts({
    arb: { options: {} },
    npm: { flatOptions: {} },
  })
  t.strictSame(result, [])
})

t.test('bundled dep with install scripts is never reported (never runs, never pending)', async t => {
  const checkAllowScripts = mockCheck(t)
  const bundled = node({
    name: 'bundled-pkg',
    version: '1.0.0',
    resolved: undefined,
    scripts: { install: 'do-stuff' },
  })
  bundled.inBundle = true

  const result = await checkAllowScripts({
    arb: arb({
      nodes: [bundled],
      // Even with an explicit allow entry, a bundled dep never runs its
      // install scripts and is never counted as pending, so the walker
      // must not flag it.
      allowScripts: { 'bundled-pkg': true },
    }),
    npm: { flatOptions: {} },
  })
  t.strictSame(result, [], 'bundled dep never flagged')
})
