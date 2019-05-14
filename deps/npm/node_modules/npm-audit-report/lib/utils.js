'use strict'

exports.severityLabel = severityLabel
exports.color = color
exports.totalVulnCount = totalVulnCount
exports.severities = severities

const ccs = require('console-control-strings')

const severityColors = {
  critical: {
    color: 'brightMagenta',
    label: 'Critical'
  },
  high: {
    color: 'brightRed',
    label: 'High'
  },
  moderate: {
    color: 'brightYellow',
    label: 'Moderate'
  },
  low: {
    color: 'bold',
    label: 'Low'
  },
  info: {
    color: '',
    label: 'Info'
  }
}

function color (value, colorName, withColor) {
  return (colorName && withColor) ? ccs.color(colorName) + value + ccs.color('reset') : value
}

function severityLabel (sev, withColor, bold) {
  if (!(sev in severityColors)) return sev.charAt(0).toUpperCase() + sev.substr(1).toLowerCase()
  let colorName = severityColors[sev].color
  if (bold) colorName = [colorName, 'bold']
  return color(severityColors[sev].label, colorName, withColor)
}

function totalVulnCount (vulns) {
  return Object.keys(vulns).reduce((accumulator, key) => {
    const vulnCount = vulns[key]
    accumulator += vulnCount

    return accumulator
  }, 0)
}

function severities (vulns) {
  return Object.keys(vulns).reduce((accumulator, severity) => {
    const vulnCount = vulns[severity]
    if (vulnCount > 0) accumulator.push([severity, vulnCount])

    return accumulator
  }, [])
}
