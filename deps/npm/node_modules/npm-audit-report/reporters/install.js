'use strict'

const Utils = require('../lib/utils')

module.exports = report
function report (data, options) {
  let msg = summary(data, options)
  if (!Object.keys(data.advisories).length) {
    return {
      report: msg,
      exitCode: 0
    }
  } else {
    msg += '\n  run `npm audit fix` to fix them, or `npm audit` for details'
    return {
      report: msg,
      exitCode: 1
    }
  }
}

module.exports.summary = summary
function summary (data, options) {
  const defaults = {
    severityThreshold: 'info'
  }

  const config = Object.assign({}, defaults, options)

  function clr (str, clr) { return Utils.color(str, clr, config.withColor) }
  function green (str) { return clr(str, 'brightGreen') }
  function red (str) { return clr(str, 'brightRed') }

  let output = ''

  const log = function (value) {
    output = output + value + '\n'
  }

  output += 'found '

  if (Object.keys(data.advisories).length === 0) {
    log(`${green('0')} vulnerabilities`)
    return output
  } else {
    let total = 0
    const sev = []

    const keys = Object.keys(data.metadata.vulnerabilities)
    for (let key of keys) {
      const value = data.metadata.vulnerabilities[key]
      total = total + value
      if (value > 0) {
        sev.push([key, value])
      }
    }

    if (sev.length > 1) {
      const severities = sev.map((value) => {
        return `${value[1]} ${Utils.severityLabel(value[0], config.withColor).toLowerCase()}`
      }).join(', ')
      log(`${red(total)} vulnerabilities (${severities})`)
    } else {
      const vulnCount = sev[0][1]
      const vulnLabel = Utils.severityLabel(sev[0][0], config.withColor).toLowerCase()
      log(`${vulnCount} ${vulnLabel} severity vulnerabilit${vulnCount === 1 ? 'y' : 'ies'}`)
    }
  }
  return output.trim()
}
