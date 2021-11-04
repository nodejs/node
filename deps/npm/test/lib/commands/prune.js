const t = require('tap')
const { real: mockNpm } = require('../../fixtures/mock-npm')

t.test('should prune using Arborist', async (t) => {
  t.plan(4)
  const { Npm } = mockNpm(t, {
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
  const npm = new Npm()
  await npm.exec('prune', [])
})
