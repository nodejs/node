// Our coverage mapping means that stuff like this doen't count for coverage.
// It does ensure that every command has a usage that renders, contains its
// name, a description, and if it has completion it is a function.  That it
// renders also ensures that any params we've defined in our commands work.
const t = require('tap')
const util = require('util')
const { load: loadMockNpm } = require('../fixtures/mock-npm.js')
const { cmdList, plumbing } = require('../../lib/utils/cmd-list.js')
const allCmds = [...cmdList, ...plumbing]

t.test('load each command', async t => {
  t.plan(allCmds.length)
  for (const cmd of allCmds.sort((a, b) => a.localeCompare(b, 'en'))) {
    t.test(cmd, async t => {
      const { npm, outputs } = await loadMockNpm(t, {
        config: { usage: true },
      })
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
      t.ok(impl.ignoreImplicitWorkspace !== undefined, 'implementation has ignoreImplictWorkspace')
      t.match(impl.usage, cmd, 'usage contains the command')
      await npm.exec(cmd, [])
      t.match(outputs[0][0], impl.usage, 'usage is what is output')
      // This ties usage to a snapshot so we have to re-run snap if usage
      // changes, which rebuilds the man pages
      t.matchSnapshot(outputs[0][0])
    })
  }
})
