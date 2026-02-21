'use strict'

const colors = require('../colors.js')
const install = require('./install.js')

module.exports = (data, { chalk }) => {
  const summary = install.summary(data, { chalk })
  const none = data.metadata.vulnerabilities.total === 0
  return none ? summary : fullReport(data, { chalk, summary })
}

const fullReport = (data, { chalk, summary }) => {
  const c = colors(chalk)
  const output = [c.white('# npm audit report'), '']

  const printed = new Set()
  for (const [, vuln] of Object.entries(data.vulnerabilities)) {
    // only print starting from the top-level advisories
    if (vuln.via.filter(v => typeof v !== 'string').length !== 0) {
      output.push(printVuln(vuln, c, data.vulnerabilities, printed))
    }
  }

  output.push(summary)

  return output.join('\n')
}

const printVuln = (vuln, c, vulnerabilities, printed, indent = '') => {
  if (printed.has(vuln)) {
    return null
  }

  printed.add(vuln)
  const output = []

  output.push(c.white(vuln.name) + '  ' + vuln.range)

  if (indent === '' && (vuln.severity !== 'low' || vuln.severity === 'info')) {
    output.push(`Severity: ${c.severity(vuln.severity)}`)
  }

  for (const via of vuln.via) {
    if (typeof via === 'string') {
      output.push(`Depends on vulnerable versions of ${c.white(via)}`)
    } else if (indent === '') {
      output.push(`${c.white(via.title)} - ${via.url}`)
    }
  }

  if (indent === '') {
    const { fixAvailable: fa } = vuln
    if (fa === false) {
      output.push(c.red('No fix available'))
    } else if (fa === true) {
      output.push(c.green('fix available') + ' via `npm audit fix`')
    } else {
      /* istanbul ignore else - should be impossible, just being cautious */
      if (typeof fa === 'object' && indent === '') {
        output.push(
          `${c.yellow('fix available')} via \`npm audit fix --force\``,
          `Will install ${fa.name}@${fa.version}` +
          `, which is ${fa.isSemVerMajor ? 'a breaking change' :
            'outside the stated dependency range'}`
        )
      }
    }
  }

  for (const path of vuln.nodes) {
    output.push(c.dim(path))
  }

  for (const effect of vuln.effects) {
    const e = printVuln(vulnerabilities[effect], c, vulnerabilities, printed, '  ')
    if (e) {
      output.push(...e.split('\n'))
    }
  }

  if (indent === '') {
    output.push('')
  }

  return output.map(l => `${indent}${l}`).join('\n')
}
