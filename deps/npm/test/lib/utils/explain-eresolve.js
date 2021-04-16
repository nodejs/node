const t = require('tap')
const npm = {}
const { explain, report } = t.mock('../../../lib/utils/explain-eresolve.js', {
  '../../../lib/npm.js': npm,
})
const { statSync, readFileSync, unlinkSync } = require('fs')
// strip out timestamps from reports
const read = f => readFileSync(f, 'utf8')
  .replace(/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}.\d{3}Z/g, '${TIME}')

const { resolve } = require('path')

const cases = require('../../fixtures/eresolve-explanations.js')

for (const [name, expl] of Object.entries(cases)) {
  // no sense storing the whole contents of each object in the snapshot
  // we can trust that JSON.stringify still works just fine.
  expl.toJSON = () => {
    return { name, json: true }
  }

  t.test(name, t => {
    npm.cache = t.testdir()
    const reportFile = resolve(npm.cache, 'eresolve-report.txt')
    t.cleanSnapshot = str => str.split(reportFile).join('${REPORT}')

    npm.color = true
    t.matchSnapshot(report(expl), 'report with color')
    const reportData = read(reportFile)
    t.matchSnapshot(reportData, 'report')
    unlinkSync(reportFile)
    t.matchSnapshot(report(expl, 2), 'report with color, depth only 2')
    t.equal(read(reportFile), reportData, 'same report written for object')
    unlinkSync(reportFile)
    npm.color = false
    t.matchSnapshot(report(expl, 6), 'report with no color, depth of 6')
    t.equal(read(reportFile), reportData, 'same report written for object')

    unlinkSync(reportFile)
    npm.color = true
    t.matchSnapshot(explain(expl), 'explain with color')
    t.throws(() => statSync(reportFile), { code: 'ENOENT' }, 'no report')
    npm.color = false
    t.matchSnapshot(explain(expl, 6), 'explain with no color, depth of 6')
    t.throws(() => statSync(reportFile), { code: 'ENOENT' }, 'no report')

    t.end()
  })
}
