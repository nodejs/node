// Our coverage mapping means that stuff like this doen't count for coverage.
// It does ensure that every command has a usage that renders, contains its
// name, a description, and if it has completion it is a function.  That it
// renders also ensures that any params we've defined in our commands work.
const t = require('tap')
const util = require('util')
const { load: loadMockNpm } = require('../fixtures/mock-npm.js')
const { commands } = require('../../lib/utils/cmd-list.js')

const isAsyncFn = (v) => typeof v === 'function' && /^\[AsyncFunction:/.test(util.inspect(v))

t.test('load each command', async t => {
  for (const cmd of commands) {
    t.test(cmd, async t => {
      const { npm, outputs, cmd: impl } = await loadMockNpm(t, {
        command: cmd,
        config: { usage: true },
      })
      const ctor = impl.constructor

      if (impl.completion) {
        t.type(impl.completion, 'function', 'completion, if present, is a function')
      }

      // exec fn
      t.ok(isAsyncFn(impl.exec), 'exec is async')
      t.ok(impl.exec.length <= 1, 'exec fn has 0 or 1 args')

      // workspaces
      t.type(ctor.ignoreImplicitWorkspace, 'boolean', 'ctor has ignoreImplictWorkspace boolean')
      t.type(ctor.workspaces, 'boolean', 'ctor has workspaces boolean')
      if (ctor.workspaces) {
        t.ok(isAsyncFn(impl.execWorkspaces), 'execWorkspaces is async')
        t.ok(impl.exec.length <= 1, 'execWorkspaces fn has 0 or 1 args')
      } else {
        t.notOk(impl.execWorkspaces, 'has no execWorkspaces fn')
      }

      // name/desc
      t.ok(impl.description, 'implementation has a description')
      t.ok(impl.name, 'implementation has a name')
      t.equal(cmd, impl.name, 'command list and name are the same')

      // usage
      t.match(impl.usage, cmd, 'usage contains the command')
      await npm.exec(cmd, [])
      t.match(outputs[0][0], impl.usage, 'usage is what is output')
    })
  }
})
