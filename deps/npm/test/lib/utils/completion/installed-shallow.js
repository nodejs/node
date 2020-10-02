const requireInject = require('require-inject')
const flatOptions = { global: false }
const npm = { flatOptions }
const t = require('tap')
const { resolve } = require('path')

const p = '../../../../lib/utils/completion/installed-shallow.js'
const installed = requireInject(p, {
  '../../../../lib/npm.js': npm
})

t.test('global not set, include globals with -g', t => {
  const dir = t.testdir({
    global: {
      node_modules: {
        x: {},
        '@scope': {
          y: {}
        }
      }
    },
    local: {
      node_modules: {
        a: {},
        '@scope': {
          b: {}
        }
      }
    }
  })
  npm.globalDir = resolve(dir, 'global/node_modules')
  npm.localDir = resolve(dir, 'local/node_modules')
  flatOptions.global = false
  const opt = { conf: { argv: { remain: [] } } }
  installed(opt, (er, res) => {
    if (er) {
      throw er
    }
    t.strictSame(res.sort(), [
      '@scope/y -g',
      'x -g',
      'a',
      '@scope/b'
    ].sort())
    t.end()
  })
})

t.test('global set, include globals and not locals', t => {
  const dir = t.testdir({
    global: {
      node_modules: {
        x: {},
        '@scope': {
          y: {}
        }
      }
    },
    local: {
      node_modules: {
        a: {},
        '@scope': {
          b: {}
        }
      }
    }
  })
  npm.globalDir = resolve(dir, 'global/node_modules')
  npm.localDir = resolve(dir, 'local/node_modules')
  flatOptions.global = true
  const opt = { conf: { argv: { remain: [] } } }
  installed(opt, (er, res) => {
    t.strictSame(res.sort(), [
      '@scope/y',
      'x'
    ].sort())
    t.end()
  })
})

t.test('more than 3 items in argv, skip it', t => {
  const dir = t.testdir({
    global: {
      node_modules: {
        x: {},
        '@scope': {
          y: {}
        }
      }
    },
    local: {
      node_modules: {
        a: {},
        '@scope': {
          b: {}
        }
      }
    }
  })
  npm.globalDir = resolve(dir, 'global/node_modules')
  npm.localDir = resolve(dir, 'local/node_modules')
  flatOptions.global = false
  const opt = { conf: { argv: { remain: [1, 2, 3, 4, 5, 6] } } }
  installed(opt, (er, res) => {
    if (er) {
      throw er
    }
    t.strictSame(res, null)
    t.end()
  })
})
