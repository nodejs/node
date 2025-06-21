const t = require('tap')
const { explain, report } = require('../../../lib/utils/explain-eresolve.js')

const cases = require('../../fixtures/eresolve-explanations.js')

t.test('basic', async t => {
  const { Chalk } = await import('chalk')
  const color = new Chalk({ level: 3 })
  const noColor = new Chalk({ level: 0 })

  for (const [name, expl] of Object.entries(cases)) {
  // no sense storing the whole contents of each object in the snapshot
  // we can trust that JSON.stringify still works just fine.
    expl.toJSON = () => ({ name, json: true })

    t.test(name, t => {
      const colorReport = report(expl, color, noColor)
      t.matchSnapshot(colorReport.explanation, 'report with color')
      t.matchSnapshot(colorReport.file, 'report from color')

      const noColorReport = report(expl, noColor, noColor)
      t.matchSnapshot(noColorReport.explanation, 'report with no color')
      t.equal(noColorReport.file, colorReport.file, 'same report written for object')

      t.matchSnapshot(explain(expl, color, 2), 'explain with color, depth of 2')
      t.matchSnapshot(explain(expl, noColor, 6), 'explain with no color, depth of 6')

      t.end()
    })
  }
})
