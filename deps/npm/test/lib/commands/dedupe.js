const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('should throw in global mode', async (t) => {
  const { npm } = await loadMockNpm(t, {
    config: {
      global: true,
    },
  })
  t.rejects(
    npm.exec('dedupe', []),
    { code: 'EDEDUPEGLOBAL' },
    'throws EDEDUPEGLOBALE'
  )
})

t.test('should remove dupes using Arborist', async (t) => {
  t.plan(5)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      '@npmcli/arborist': function (args) {
        t.ok(args, 'gets options object')
        t.ok(args.path, 'gets path option')
        t.ok(args.dryRun, 'gets dryRun from user')
        this.dedupe = () => {
          t.ok(true, 'dedupe is called')
        }
      },
      '../../lib/utils/reify-finish.js': (npm, arb) => {
        t.ok(arb, 'gets arborist tree')
      },
    },
    config: {
      'dry-run': 'true',
    },
  })
  await npm.exec('dedupe', [])
})

t.test('should remove dupes using Arborist - no arguments', async (t) => {
  t.plan(1)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      '@npmcli/arborist': function (args) {
        t.ok(args.dryRun, 'gets dryRun from config')
        this.dedupe = () => {}
      },
      '../../lib/utils/reify-output.js': () => {},
      '../../lib/utils/reify-finish.js': () => {},
    },
    config: {
      'dry-run': true,
    },
  })
  await npm.exec('dedupe', [])
})
