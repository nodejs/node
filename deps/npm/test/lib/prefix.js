const t = require('tap')

t.test('prefix', (t) => {
  t.plan(3)
  const dir = '/prefix/dir'

  const Prefix = require('../../lib/prefix.js')
  const prefix = new Prefix({
    prefix: dir,
    output: (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })

  prefix.exec([], (err) => {
    t.error(err, 'npm prefix')
    t.ok('should have printed directory')
  })
})
