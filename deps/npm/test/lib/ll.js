const t = require('tap')

t.test('ll', t => {
  t.plan(3)

  class LS {
    constructor (npm) {
      this.npm = npm
    }

    exec (args, cb) {
      t.same(args, ['pkg'], 'should forward args')
      cb()
    }
  }

  const LL = t.mock('../../lib/ll.js', {
    '../../lib/ls.js': LS,
  })
  const ll = new LL({
    config: {
      set: (key, value) => {
        t.equal(key, 'long', 'should set long config value')
        t.equal(value, true, 'should set a truthy value')
      },
    },
  })

  ll.exec(['pkg'], err => {
    if (err)
      throw err
  })
})
