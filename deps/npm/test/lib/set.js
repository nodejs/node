const t = require('tap')

let configArgs = null
const npm = {
  commands: {
    config: (args, cb) => {
      configArgs = args
      cb()
    },
  },
}

const Set = t.mock('../../lib/set.js')
const set = new Set(npm)

t.test('npm set - no args', t => {
  set.exec([], (err) => {
    t.match(err, /npm set/, 'prints usage')
    t.end()
  })
})

t.test('npm set', t => {
  set.exec(['email', 'me@me.me'], (err) => {
    if (err)
      throw err

    t.strictSame(configArgs, ['set', 'email', 'me@me.me'], 'passed the correct arguments to config')
    t.end()
  })
})
