const t = require('tap')

t.test('unstar', async t => {
  t.plan(3)

  class Star {
    constructor (npm) {
      this.npm = npm
    }

    async exec (args) {
      t.same(args, ['pkg'], 'should forward packages')
    }
  }
  const Unstar = t.mock('../../../lib/commands/unstar.js', {
    '../../../lib/commands/star.js': Star,
  })

  const unstar = new Unstar({
    config: {
      set: (key, value) => {
        t.equal(key, 'star.unstar', 'should set unstar config value')
        t.equal(value, true, 'should set a truthy value')
      },
    },
  })

  await unstar.exec(['pkg'])
})
