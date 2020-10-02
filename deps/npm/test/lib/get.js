const { test } = require('tap')
const requireInject = require('require-inject')

test('should retrieve values from npm.commands.config', (t) => {
  const get = requireInject('../../lib/get.js', {
    '../../lib/npm.js': {
      commands: {
        config: ([action, arg]) => {
          t.equal(action, 'get', 'should use config get action')
          t.equal(arg, 'foo', 'should use expected key')
          t.end()
        }
      }
    }
  })

  get(['foo'])
})
