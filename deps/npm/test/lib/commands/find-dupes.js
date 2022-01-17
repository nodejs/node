const t = require('tap')

const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('should run dedupe in dryRun mode', async (t) => {
  t.plan(5)
  const { npm } = await loadMockNpm(t, {
    mocks: {
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
    },
    config: {
      // explicitly set to false so we can be 100% sure it's always true when it
      // hits arborist
      'dry-run': false,
    },
  })
  await npm.exec('find-dupes', [])
})
