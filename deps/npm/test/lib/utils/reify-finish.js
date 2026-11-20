const t = require('tap')
const fs = require('node:fs/promises')
const { join } = require('node:path')
const { cleanNewlines } = require('../../fixtures/clean-snapshot')
const tmock = require('../../fixtures/tmock')
const mockNpm = require('../../fixtures/mock-npm')

// windowwwwwwssss!!!!!
const readRc = async (dir) => {
  const res = await fs.readFile(join(dir, 'npmrc'), 'utf8').catch(() => '')
  return cleanNewlines(res).trim()
}

const mockReififyFinish = async (t, { actualTree = {}, otherDirs = {}, ...config }) => {
  const mock = await mockNpm(t, {
    npm: ({ other }) => ({
      npmRoot: other,
    }),
    otherDirs: {
      npmrc: `key=value`,
      ...otherDirs,
    },
    config,
  })

  let reportedScripts
  const reifyFinish = tmock(t, '{LIB}/utils/reify-finish.js', {
    '{LIB}/utils/reify-output.js': (_npm, _arb, { unreviewedScripts }) => {
      reportedScripts = unreviewedScripts
    },
  })

  await reifyFinish(mock.npm, {
    options: { global: mock.npm.global },
    actualTree: typeof actualTree === 'function' ? actualTree(mock) : actualTree,
  })

  const builtinRc = {
    raw: await readRc(mock.other),
    data: Object.fromEntries(Object.entries(mock.npm.config.data.get('builtin').data)),
  }

  return {
    builtinRc,
    reportedScripts,
    ...mock,
  }
}

t.test('ok by default', async t => {
  const mock = await mockReififyFinish(t, {
    global: false,
  })
  t.same(mock.builtinRc.raw, 'key=value')
  t.strictSame(mock.builtinRc.data, { key: 'value' })
})

t.test('only omits disconnected orphans from post-install reporting', async t => {
  for (const strict of [false, true]) {
    await t.test(`strict=${strict}`, async t => {
      const candidate = (name, properties = {}) => ({
        name,
        package: { name, version: '1.0.0', scripts: { install: `echo ${name}` } },
        extraneous: true,
        parent: null,
        linksIn: new Set(),
        ...properties,
      })
      const orphan = candidate('orphan')
      const attached = candidate('attached', { parent: {} })
      const linked = candidate('linked', { linksIn: new Set([{}]) })
      const { reportedScripts } = await mockReififyFinish(t, {
        'strict-allow-scripts': strict,
        actualTree: {
          inventory: new Map([orphan, attached, linked].map(n => [n.name, n])),
        },
      })
      t.strictSame(reportedScripts.map(({ node }) => node), [attached, linked])
    })
  }
})

t.test('should not write if no global npm module', async t => {
  const mock = await mockReififyFinish(t, {
    global: true,
    actualTree: {
      inventory: new Map(),
    },
  })
  t.same(mock.builtinRc.raw, 'key=value')
  t.strictSame(mock.builtinRc.data, { key: 'value' })
})

t.test('should not write if builtin conf had load error', async t => {
  const mock = await mockReififyFinish(t, {
    global: true,
    otherDirs: {
      npmrc: {},
    },
    actualTree: {
      inventory: new Map([['node_modules/npm', {}]]),
    },
  })
  t.same(mock.builtinRc.raw, '')
  t.strictSame(mock.builtinRc.data, {})
})

t.test('should write if everything above passes', async t => {
  const mock = await mockReififyFinish(t, {
    global: true,
    otherDirs: {
      'new-npm': {},
    },
    actualTree: ({ other }) => ({
      inventory: new Map([['node_modules/npm', { path: join(other, 'new-npm') }]]),
    }),
  })

  t.same(mock.builtinRc.raw, 'key=value')
  t.strictSame(mock.builtinRc.data, { key: 'value' })

  const newFile = await readRc(join(mock.other, 'new-npm'))
  t.equal(mock.builtinRc.raw, newFile)
})
