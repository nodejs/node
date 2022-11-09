const t = require('tap')
const { resolve } = require('path')
const { explain, report } = require('../../../lib/utils/explain-eresolve.js')

const cases = require('../../fixtures/eresolve-explanations.js')
const { cleanDate } = require('../../fixtures/clean-snapshot.js')

for (const [name, expl] of Object.entries(cases)) {
  // no sense storing the whole contents of each object in the snapshot
  // we can trust that JSON.stringify still works just fine.
  expl.toJSON = () => ({ name, json: true })

  t.test(name, t => {
    const dir = t.testdir()
    const fileReport = resolve(dir, 'eresolve-report.txt')
    const opts = { file: fileReport, date: new Date().toISOString() }

    t.cleanSnapshot = str => cleanDate(str.split(fileReport).join('${REPORT}'))

    const color = report(expl, true, opts)
    t.matchSnapshot(color.explanation, 'report with color')
    t.matchSnapshot(color.file, 'report from color')

    const noColor = report(expl, false, opts)
    t.matchSnapshot(noColor.explanation, 'report with no color')
    t.equal(noColor.file, color.file, 'same report written for object')

    t.matchSnapshot(explain(expl, true, 2), 'explain with color, depth of 2')
    t.matchSnapshot(explain(expl, false, 6), 'explain with no color, depth of 6')

    t.end()
  })
}
