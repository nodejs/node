const { test } = require('tap')
const findDupes = require('../../lib/find-dupes.js')
const requireInject = require('require-inject')

test('should run dedupe in dryRun mode', (t) => {
  const findDupes = requireInject('../../lib/find-dupes.js', {
    '../../lib/dedupe.js': function (args, cb) {
      t.ok(args.dryRun, 'dryRun is true')
      cb()
    }
  })
  findDupes(null, () => {
    t.ok(true, 'callback is called')
    t.end()
  })
})

