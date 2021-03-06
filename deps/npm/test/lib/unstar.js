const requireInject = require('require-inject')
const t = require('tap')

t.test('unstar', t => {
  t.plan(3)

  class Star {
    constructor (npm) {
      this.npm = npm
    }

    exec (args, cb) {
      t.deepEqual(args, ['pkg'], 'should forward packages')
      cb()
    }
  }
  const Unstar = requireInject('../../lib/unstar.js', {
    '../../lib/star.js': Star,
  })

  const unstar = new Unstar({
    config: {
      set: (key, value) => {
        t.equal(key, 'star.unstar', 'should set unstar config value')
        t.equal(value, true, 'should set a truthy value')
      },
    },
  })

  unstar.exec(['pkg'], err => {
    if (err)
      throw err
  })
})
