const t = require('tap')

const FindDupes = require('../../lib/find-dupes.js')

t.test('should run dedupe in dryRun mode', (t) => {
  t.plan(3)
  const findDupesTest = new FindDupes({
    config: {
      set: (k, v) => {
        t.match(k, 'dry-run')
        t.match(v, true)
      },
    },
    commands: {
      dedupe: (args, cb) => {
        t.match(args, [])
        cb()
      },
    },
  })
  findDupesTest.exec({}, () => {
    t.end()
  })
})
