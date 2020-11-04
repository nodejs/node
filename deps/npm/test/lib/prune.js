const { test } = require('tap')
const prune = require('../../lib/prune.js')
const requireInject = require('require-inject')

test('should prune using Arborist', (t) => {
  const prune = requireInject('../../lib/prune.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        'foo': 'bar'
      }
    },
    '@npmcli/arborist': function (args) {
      t.ok(args, 'gets options object')
      t.ok(args.path, 'gets path option')
      this.prune = () => {
        t.ok(true, 'prune is called')
      }
    },
    '../../lib/utils/reify-output.js': (arb) => {
      t.ok(arb, 'gets arborist tree')
    }
  })
  prune(null, () => {
    t.ok(true, 'callback is called')
    t.end()
  })
})

