const t = require('tap')

const preflight = require('../../../lib/utils/strict-allow-scripts-preflight.js')

// Build a node fixture that checkAllowScripts will pick up as "unreviewed":
// registry-resolved, hasInstallScript true, not project root / workspace /
// link, and no allowScripts entry covering it.
const node = ({
  name = 'pkg',
  version = '1.0.0',
  scripts = { install: 'node-gyp rebuild' },
} = {}) => ({
  name,
  resolved: `https://registry.npmjs.org/${name}/-/${name}-${version}.tgz`,
  hasInstallScript: !!Object.keys(scripts).length,
  path: `/fake/${name}`,
  isProjectRoot: false,
  isWorkspace: false,
  isLink: false,
  package: { name, version, scripts },
})

const tree = (nodes) => ({
  inventory: new Map(nodes.map((n, i) => [`node_modules/${n.name}-${i}`, n])),
})

const makeArb = ({ ideal, actual, allowScripts = null } = {}) => {
  const arb = {
    options: { allowScripts, ignoreScripts: false },
    idealTree: ideal ?? null,
    actualTree: actual ?? null,
  }
  arb.buildIdealTree = async () => arb.idealTree
  return arb
}

t.test('no-op when strictAllowScripts is not set', async t => {
  const arb = makeArb({ ideal: tree([node()]) })
  await preflight({ arb, npm: { flatOptions: {} }, idealTreeOpts: {} })
  t.pass('returned without throwing')
})

t.test('no-op when dangerouslyAllowAllScripts overrides', async t => {
  const arb = makeArb({ ideal: tree([node()]) })
  await preflight({
    arb,
    npm: { flatOptions: { strictAllowScripts: true, dangerouslyAllowAllScripts: true } },
    idealTreeOpts: {},
  })
  t.pass('returned without throwing')
})

t.test('no-op when ignoreScripts overrides', async t => {
  const arb = makeArb({ ideal: tree([node()]) })
  arb.options.ignoreScripts = true
  await preflight({
    arb,
    npm: { flatOptions: { strictAllowScripts: true } },
    idealTreeOpts: {},
  })
  t.pass('returned without throwing')
})

t.test('throws when unreviewed install scripts exist (idealTree path)', async t => {
  const arb = makeArb({ ideal: tree([node({ name: 'canvas' }), node({ name: 'sharp' })]) })
  await t.rejects(
    preflight({
      arb,
      npm: { flatOptions: { strictAllowScripts: true } },
      idealTreeOpts: {},
    }),
    {
      code: 'ESTRICTALLOWSCRIPTS',
      message: /2 package\(s\) have install scripts not covered/,
    }
  )
})

t.test('passes when all install-script nodes are explicitly approved', async t => {
  const arb = makeArb({
    ideal: tree([node({ name: 'canvas' })]),
    allowScripts: { canvas: true },
  })
  await preflight({
    arb,
    npm: { flatOptions: { strictAllowScripts: true } },
    idealTreeOpts: {},
  })
  t.pass('no error thrown')
})

t.test('passes when all install-script nodes are explicitly denied', async t => {
  const arb = makeArb({
    ideal: tree([node({ name: 'canvas' })]),
    allowScripts: { canvas: false },
  })
  await preflight({
    arb,
    npm: { flatOptions: { strictAllowScripts: true } },
    idealTreeOpts: {},
  })
  t.pass('no error thrown')
})

t.test('skips buildIdealTree when arb.idealTree already exists (npm ci path)', async t => {
  // `npm ci` builds the ideal tree before calling the preflight. The
  // helper must not rebuild it.
  const ideal = tree([node({ name: 'pre-built' })])
  const arb = makeArb({ ideal, allowScripts: { 'pre-built': true } })
  let buildCalls = 0
  arb.buildIdealTree = async () => {
    buildCalls++
    return arb.idealTree
  }
  await preflight({
    arb,
    npm: { flatOptions: { strictAllowScripts: true } },
    idealTreeOpts: {},
  })
  t.equal(buildCalls, 0, 'buildIdealTree was not called a second time')
})

t.test('builds the ideal tree when arb.idealTree is empty (npm install path)', async t => {
  // `npm install` does not pre-build the ideal tree. The helper must
  // build it so checkAllowScripts has something to walk.
  const arb = makeArb({ allowScripts: { 'fresh-pkg': true } })
  let buildCalls = 0
  arb.buildIdealTree = async () => {
    buildCalls++
    arb.idealTree = tree([node({ name: 'fresh-pkg' })])
  }
  await preflight({
    arb,
    npm: { flatOptions: { strictAllowScripts: true } },
    idealTreeOpts: {},
  })
  t.equal(buildCalls, 1, 'buildIdealTree was called once')
})

t.test('uses actualTree when idealTreeOpts is not provided (rebuild path)', async t => {
  const arb = makeArb({ actual: tree([node({ name: 'rebuild-pkg' })]) })
  await t.rejects(
    preflight({
      arb,
      npm: { flatOptions: { strictAllowScripts: true } },
    }),
    {
      code: 'ESTRICTALLOWSCRIPTS',
      message: /rebuild-pkg@1\.0\.0/,
    }
  )
})

t.test('error message includes script bodies', async t => {
  const arb = makeArb({
    ideal: tree([node({ name: 'canvas', version: '2.11.0', scripts: { install: 'node-gyp rebuild' } })]),
  })
  await t.rejects(
    preflight({
      arb,
      npm: { flatOptions: { strictAllowScripts: true } },
      idealTreeOpts: {},
    }),
    { message: /canvas@2\.11\.0 \(install: node-gyp rebuild\)/ }
  )
})

t.test('error label falls back to node.name when package.version is missing', async t => {
  // Exercises the `version ? '${name}@${version}' : name` branch in the
  // error formatter when a node has no package.version (and the name
  // falls back to node.name via `node.package?.name || node.name`).
  const bare = {
    name: 'no-version-pkg',
    resolved: 'https://registry.npmjs.org/no-version-pkg/-/no-version-pkg-1.0.0.tgz',
    hasInstallScript: true,
    path: '/fake/no-version-pkg',
    isProjectRoot: false,
    isWorkspace: false,
    isLink: false,
    package: { scripts: { install: 'node-gyp rebuild' } },
  }
  const arb = makeArb({ ideal: tree([bare]) })
  await t.rejects(
    preflight({
      arb,
      npm: { flatOptions: { strictAllowScripts: true } },
      idealTreeOpts: {},
    }),
    { message: /no-version-pkg \(install: node-gyp rebuild\)/ }
  )
})

t.test('project-scoped error suggests approve-scripts / deny-scripts', async t => {
  const arb = makeArb({ ideal: tree([node({ name: 'canvas' })]) })
  await t.rejects(
    preflight({
      arb,
      npm: { flatOptions: { strictAllowScripts: true } },
      idealTreeOpts: {},
    }),
    { message: /Approve them with `npm approve-scripts`/ }
  )
})

t.test('global error points at --allow-scripts, not approve-scripts', async t => {
  const arb = makeArb({ ideal: tree([node({ name: 'canvas' })]) })
  await t.rejects(
    preflight({
      arb,
      npm: { global: true, flatOptions: { strictAllowScripts: true } },
      idealTreeOpts: {},
    }),
    (err) => {
      t.equal(err.code, 'ESTRICTALLOWSCRIPTS')
      t.match(err.message, /--allow-scripts/)
      t.match(err.message, /npm config set allow-scripts=canvas/)
      t.notMatch(err.message, /approve-scripts/)
      return true
    }
  )
})
