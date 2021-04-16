const flatOptions = { global: false }
const npm = { flatOptions }
const t = require('tap')
const { resolve } = require('path')

const p = '../../../../lib/utils/completion/installed-shallow.js'
const installed = require(p)

t.test('global not set, include globals with -g', async t => {
  const dir = t.testdir({
    global: {
      node_modules: {
        x: {},
        '@scope': {
          y: {},
        },
      },
    },
    local: {
      node_modules: {
        a: {},
        '@scope': {
          b: {},
        },
      },
    },
  })
  npm.globalDir = resolve(dir, 'global/node_modules')
  npm.localDir = resolve(dir, 'local/node_modules')
  flatOptions.global = false
  const opt = { conf: { argv: { remain: [] } } }
  const res = await installed(npm, opt)
  t.strictSame(res.sort(), [
    '@scope/y -g',
    'x -g',
    'a',
    '@scope/b',
  ].sort())
  t.end()
})

t.test('global set, include globals and not locals', async t => {
  const dir = t.testdir({
    global: {
      node_modules: {
        x: {},
        '@scope': {
          y: {},
        },
      },
    },
    local: {
      node_modules: {
        a: {},
        '@scope': {
          b: {},
        },
      },
    },
  })
  npm.globalDir = resolve(dir, 'global/node_modules')
  npm.localDir = resolve(dir, 'local/node_modules')
  flatOptions.global = true
  const opt = { conf: { argv: { remain: [] } } }
  const res = await installed(npm, opt)
  t.strictSame(res.sort(), [
    '@scope/y',
    'x',
  ].sort())
  t.end()
})

t.test('more than 3 items in argv, skip it', async t => {
  const dir = t.testdir({
    global: {
      node_modules: {
        x: {},
        '@scope': {
          y: {},
        },
      },
    },
    local: {
      node_modules: {
        a: {},
        '@scope': {
          b: {},
        },
      },
    },
  })
  npm.globalDir = resolve(dir, 'global/node_modules')
  npm.localDir = resolve(dir, 'local/node_modules')
  flatOptions.global = false
  const opt = { conf: { argv: { remain: [1, 2, 3, 4, 5, 6] } } }
  const res = await installed(npm, opt)
  t.strictSame(res, null)
  t.end()
})
