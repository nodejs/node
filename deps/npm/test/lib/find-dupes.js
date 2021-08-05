const t = require('tap')

const { real: mockNpm } = require('../fixtures/mock-npm')

t.test('should run dedupe in dryRun mode', async (t) => {
  t.plan(5)
  const { npm, command } = mockNpm(t, {
    '@npmcli/arborist': function (args) {
      t.ok(args, 'gets options object')
      t.ok(args.path, 'gets path option')
      t.ok(args.dryRun, 'is called in dryRun mode')
      this.dedupe = () => {
        t.ok(true, 'dedupe is called')
      }
    },
    '../../lib/utils/reify-finish.js': (npm, arb) => {
      t.ok(arb, 'gets arborist tree')
    },
  })
  await npm.load()
  // explicitly set to false so we can be 100% sure it's always true when it
  // hits arborist
  npm.config.set('dry-run', false)
  npm.config.set('prefix', 'foo')
  await command('find-dupes')
})
