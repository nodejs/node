const t = require('tap')

t.test('root', (t) => {
  t.plan(3)
  const dir = '/root/dir'

  const Root = require('../../lib/root.js')
  const root = new Root({
    dir,
    output: (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })

  root.exec([], (err) => {
    t.error(err, 'npm root')
    t.ok('should have printed directory')
  })
})
