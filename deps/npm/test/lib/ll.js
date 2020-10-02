const t = require('tap')
const requireInject = require('require-inject')
const configs = {}
let lsCalled = false
const ll = requireInject('../../lib/ll.js', {
  '../../lib/npm.js': {
    config: {
      set: (k, v) => {
        configs[k] = v
      }
    },
    commands: {
      ls: (args, cb) => {
        lsCalled = true
        cb()
      }
    }
  }
})

const ls = require('../../lib/ls.js')
const { usage, completion } = ls
t.equal(ll.usage, usage)
t.equal(ll.completion.toString(), completion.toString())
t.test('the ll command', t => {
  ll([], () => {
    t.equal(lsCalled, true)
    t.strictSame(configs, { long: true })
    t.end()
  })
})
