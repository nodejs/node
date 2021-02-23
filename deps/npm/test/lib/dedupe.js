const { test } = require('tap')
const requireInject = require('require-inject')

test('should throw in global mode', (t) => {
  const dedupe = requireInject('../../lib/dedupe.js', {
    '../../lib/npm.js': {
      flatOptions: {
        global: true,
      },
    },
  })

  dedupe([], er => {
    t.match(er, { code: 'EDEDUPEGLOBAL' }, 'throws EDEDUPEGLOBAL')
    t.end()
  })
})

test('should remove dupes using Arborist', (t) => {
  const dedupe = requireInject('../../lib/dedupe.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        dryRun: 'false',
      },
    },
    '@npmcli/arborist': function (args) {
      t.ok(args, 'gets options object')
      t.ok(args.path, 'gets path option')
      t.ok(args.dryRun, 'gets dryRun from user')
      this.dedupe = () => {
        t.ok(true, 'dedupe is called')
      }
    },
    '../../lib/utils/reify-finish.js': (arb) => {
      t.ok(arb, 'gets arborist tree')
    },
  })
  dedupe({ dryRun: true }, er => {
    if (er)
      throw er
    t.ok(true, 'callback is called')
    t.end()
  })
})

test('should remove dupes using Arborist - no arguments', (t) => {
  const dedupe = requireInject('../../lib/dedupe.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        dryRun: 'true',
      },
    },
    '@npmcli/arborist': function (args) {
      t.ok(args.dryRun, 'gets dryRun from flatOptions')
      this.dedupe = () => {}
    },
    '../../lib/utils/reify-output.js': () => {},
  })
  dedupe(null, () => {
    t.end()
  })
})
