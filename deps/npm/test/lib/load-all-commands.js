// Our coverage mapping means that stuff like this doen't count for coverage.
// It does ensure that every command has a usage that renders, contains its
// name, a description, and if it has completion it is a function.  That it
// renders also ensures that any params we've defined in our commands work.
const t = require('tap')
const util = require('util')
const { real: mockNpm } = require('../fixtures/mock-npm.js')
const { cmdList } = require('../../lib/utils/cmd-list.js')

const { Npm, outputs } = mockNpm(t)
const npm = new Npm()

t.test('load each command', async t => {
  t.afterEach(() => {
    outputs.length = 0
  })
  t.plan(cmdList.length)
  await npm.load()
  npm.config.set('usage', true) // This makes npm.exec output the usage
  for (const cmd of cmdList.sort((a, b) => a.localeCompare(b, 'en'))) {
    t.test(cmd, async t => {
      const impl = await npm.cmd(cmd)
      if (impl.completion) {
        t.type(impl.completion, 'function', 'completion, if present, is a function')
      }
      t.type(impl.exec, 'function', 'implementation has an exec function')
      t.type(impl.execWorkspaces, 'function', 'implementation has an execWorkspaces function')
      t.equal(util.inspect(impl.exec), '[AsyncFunction: exec]', 'exec function is async')
      t.equal(
        util.inspect(impl.execWorkspaces),
        '[AsyncFunction: execWorkspaces]',
        'execWorkspaces function is async'
      )
      t.ok(impl.description, 'implementation has a description')
      t.ok(impl.name, 'implementation has a name')
      t.match(impl.usage, cmd, 'usage contains the command')
      await npm.exec(cmd, [])
      t.match(outputs[0][0], impl.usage, 'usage is what is output')
      // This ties usage to a snapshot so we have to re-run snap if usage
      // changes, which rebuilds the man pages
      t.matchSnapshot(outputs[0][0])
    })
  }
})
