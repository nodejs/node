const t = require('tap')
const mockNpm = require('../fixtures/mock-npm')

t.test('should throw in global mode', (t) => {
  const Dedupe = t.mock('../../lib/dedupe.js')
  const npm = mockNpm({
    config: { 'dry-run': false, global: true },
  })
  const dedupe = new Dedupe(npm)

  dedupe.exec([], er => {
    t.match(er, { code: 'EDEDUPEGLOBAL' }, 'throws EDEDUPEGLOBAL')
    t.end()
  })
})

t.test('should remove dupes using Arborist', (t) => {
  const Dedupe = t.mock('../../lib/dedupe.js', {
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
  })
  const npm = mockNpm({
    prefix: 'foo',
    config: {
      'dry-run': 'true',
    },
  })
  const dedupe = new Dedupe(npm)
  dedupe.exec([], er => {
    if (er)
      throw er
    t.ok(true, 'callback is called')
    t.end()
  })
})

t.test('should remove dupes using Arborist - no arguments', (t) => {
  const Dedupe = t.mock('../../lib/dedupe.js', {
    '@npmcli/arborist': function (args) {
      t.ok(args.dryRun, 'gets dryRun from config')
      this.dedupe = () => {}
    },
    '../../lib/utils/reify-output.js': () => {},
  })
  const npm = mockNpm({
    prefix: 'foo',
    config: {
      'dry-run': 'true',
    },
  })
  const dedupe = new Dedupe(npm)
  dedupe.exec(null, () => {
    t.end()
  })
})
