const t = require('tap')

t.test('ll', t => {
  t.plan(3)

  class LS {
    constructor (npm) {
      this.npm = npm
    }

    async exec (args) {
      t.same(args, ['pkg'], 'should forward args')
    }
  }

  const LL = t.mock('../../../lib/commands/ll.js', {
    '../../../lib/commands/ls.js': LS,
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
    if (err) {
      throw err
    }
  })
})
