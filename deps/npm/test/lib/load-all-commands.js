// Our coverage mapping means that stuff like this doen't count for coverage.
// It does ensure that every command has a usage that renders, contains its
// name, a description, and if it has completion it is a function.  That it
// renders also ensures that any params we've defined in our commands work.
const t = require('tap')
const { real: mockNpm } = require('../fixtures/mock-npm.js')
const { cmdList } = require('../../lib/utils/cmd-list.js')

const { npm, outputs } = mockNpm(t)

t.test('load each command', t => {
  t.plan(cmdList.length)
  npm.load((er) => {
    if (er)
      throw er
    npm.config.set('usage', true)
    for (const cmd of cmdList.sort((a, b) => a.localeCompare(b, 'en'))) {
      t.test(cmd, t => {
        const impl = npm.commands[cmd]
        if (impl.completion)
          t.type(impl.completion, 'function', 'completion, if present, is a function')
        t.type(impl, 'function', 'implementation is a function')
        t.ok(impl.description, 'implementation has a description')
        t.ok(impl.name, 'implementation has a name')
        t.match(impl.usage, cmd, 'usage contains the command')
        impl([], (err) => {
          t.notOk(err)
          t.match(outputs[0][0], impl.usage, 'usage is what is output')
          // This ties usage to a snapshot so we have to re-run snap if usage
          // changes, which rebuilds the man pages
          t.matchSnapshot(outputs[0][0])
          t.end()
        })
      })
      outputs.length = 0
    }
  })
})
