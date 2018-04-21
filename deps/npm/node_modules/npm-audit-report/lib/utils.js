'use strict'

exports.severityLabel = severityLabel
exports.color = color

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
  }
}

function color (value, colorName, withColor) {
  return (colorName && withColor) ? ccs.color(colorName) + value + ccs.color('reset') : value
}

function severityLabel (sev, withColor, bold) {
  let colorName = severityColors[sev].color
  if (bold) colorName = [colorName, 'bold']
  return color(severityColors[sev].label, colorName, withColor)
}
