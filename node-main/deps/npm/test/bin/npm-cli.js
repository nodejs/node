const t = require('tap')
const tmock = require('../fixtures/tmock')

t.test('loading the bin calls the implementation', t => {
  tmock(t, '{BIN}/npm-cli.js', {
    '{LIB}/cli.js': proc => {
      t.equal(proc, process, 'called implementation with process object')
      t.end()
    },
  })
})
