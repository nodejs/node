const t = require('tap')
t.test('loading the bin calls the implementation', t => {
  t.mock('../../bin/npm-cli.js', {
    '../../lib/cli.js': proc => {
      t.equal(proc, process, 'called implementation with process object')
      t.end()
    },
  })
})
