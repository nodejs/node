const t = require('tap')

t.test('should prune using Arborist', (t) => {
  const Prune = t.mock('../../lib/prune.js', {
    '@npmcli/arborist': function (args) {
      t.ok(args, 'gets options object')
      t.ok(args.path, 'gets path option')
      this.prune = () => {
        t.ok(true, 'prune is called')
      }
    },
    '../../lib/utils/reify-finish.js': (arb) => {
      t.ok(arb, 'gets arborist tree')
    },
  })
  const prune = new Prune({
    prefix: 'foo',
    flatOptions: {
      foo: 'bar',
    },
  })
  prune.exec(null, er => {
    if (er)
      throw er
    t.ok(true, 'callback is called')
    t.end()
  })
})
