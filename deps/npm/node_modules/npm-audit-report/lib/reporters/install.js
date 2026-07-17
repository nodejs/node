const colors = require('../colors.js')

const calculate = (data, { chalk }) => {
  const c = colors(chalk)
  const output = []
  const { metadata: { vulnerabilities } } = data
  const vulnCount = vulnerabilities.total

  let someFixable = false
  let someForceFixable = false
  let forceFixSemVerMajor = false
  let someUnfixable = false

  if (vulnCount === 0) {
    output.push(`found ${c.green('0')} vulnerabilities`)
  } else {
    for (const [, vuln] of Object.entries(data.vulnerabilities)) {
      const { fixAvailable } = vuln
      someFixable = someFixable || fixAvailable === true
      someUnfixable = someUnfixable || fixAvailable === false
      if (typeof fixAvailable === 'object') {
        someForceFixable = true
        forceFixSemVerMajor = forceFixSemVerMajor || fixAvailable.isSemVerMajor
      }
    }
    const total = vulnerabilities.total
    const sevs = Object.entries(vulnerabilities).filter(([s, count]) => {
      return (s === 'low' || s === 'moderate' || s === 'high' || s === 'critical') &&
        count > 0
    })

    if (sevs.length > 1) {
      const severities = sevs.map(([s, count]) => {
        return `${count} ${c.severity(s)}`
      }).join(', ')
      output.push(`${c.red(total)} vulnerabilities (${severities})`)
    } else {
      const [sev, count] = sevs[0]
      output.push(`${count} ${c.severity(sev)} severity vulnerabilit${count === 1 ? 'y' : 'ies'}`)
    }

    // XXX use a different footer line if some aren't fixable easily.
    // just 'run `npm audit` for details' maybe?

    if (someFixable) {
      output.push('', 'To address ' +
        (someForceFixable || someUnfixable ? 'issues that do not require attention'
        : 'all issues') + ', run:\n  npm audit fix')
    }

    if (someForceFixable) {
      output.push('', 'To address all issues' +
        (someUnfixable ? ' possible' : '') +
        (forceFixSemVerMajor ? ' (including breaking changes)' : '') +
        ', run:\n  npm audit fix --force')
    }

    if (someUnfixable) {
      output.push('',
        'Some issues need review, and may require choosing',
        'a different dependency.')
    }
  }

  const summary = output.join('\n')
  return {
    summary,
    report: vulnCount > 0 ? `${summary}\n\nRun \`npm audit\` for details.`
    : summary,
  }
}

module.exports = Object.assign((data, opt) => calculate(data, opt).report, {
  summary: (data, opt) => calculate(data, opt).summary,
})
