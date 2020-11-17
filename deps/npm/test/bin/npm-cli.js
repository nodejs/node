const t = require('tap')
const requireInject = require('require-inject')
t.test('loading the bin calls the implementation', t => {
  requireInject('../../bin/npm-cli.js', {
    '../../lib/cli.js': proc => {
      t.equal(proc, process, 'called implementation with process object')
      t.end()
    },
  })
})
