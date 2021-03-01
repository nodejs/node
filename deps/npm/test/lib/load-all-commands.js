// just for gathering coverage info
const npm = require('../../lib/npm.js')
const t = require('tap')
const { cmdList } = require('../../lib/utils/cmd-list.js')

t.test('load npm', t => npm.load(er => {
  if (er)
    throw er
}))

t.test('load each command', t => {
  t.plan(cmdList.length)
  for (const cmd of cmdList.sort((a, b) => a.localeCompare(b))) {
    t.test(cmd, t => {
      const impl = npm.commands[cmd]
      if (impl.completion) {
        t.plan(3)
        t.isa(impl.completion, 'function', 'completion, if present, is a function')
      } else
        t.plan(2)
      t.isa(impl, 'function', 'implementation is a function')
      t.isa(impl.usage, 'string', 'usage is a string')
    })
  }
})
