const chalk = require('chalk')
const nocolor = {
  bold: s => s,
  dim: s => s,
  red: s => s,
  yellow: s => s,
  cyan: s => s,
  magenta: s => s,
}

const explainNode = (node, depth, color) =>
  printNode(node, color) +
  explainDependents(node, depth, color)

const colorType = (type, color) => {
  const { red, yellow, cyan, magenta } = color ? chalk : nocolor
  const style = type === 'extraneous' ? red
    : type === 'dev' ? yellow
    : type === 'optional' ? cyan
    : type === 'peer' ? magenta
    : /* istanbul ignore next */ s => s
  return style(type)
}

const printNode = (node, color) => {
  const {
    name,
    version,
    location,
    extraneous,
    dev,
    optional,
    peer,
  } = node
  const { bold, dim } = color ? chalk : nocolor
  const extra = []
  if (extraneous)
    extra.push(' ' + bold(colorType('extraneous', color)))

  if (dev)
    extra.push(' ' + bold(colorType('dev', color)))

  if (optional)
    extra.push(' ' + bold(colorType('optional', color)))

  if (peer)
    extra.push(' ' + bold(colorType('peer', color)))

  return `${bold(name)}@${bold(version)}${extra.join('')}` +
    (location ? dim(`\n${location}`) : '')
}

const explainDependents = ({ name, dependents }, depth, color) => {
  if (!dependents || !dependents.length || depth <= 0)
    return ''

  const max = Math.ceil(depth / 2)
  const messages = dependents.slice(0, max)
    .map(edge => explainEdge(edge, depth, color))

  // show just the names of the first 5 deps that overflowed the list
  if (dependents.length > max) {
    let len = 0
    const maxLen = 50
    const showNames = []
    for (let i = max; i < dependents.length; i++) {
      const { from: { name = 'the root project' } } = dependents[i]
      len += name.length
      if (len >= maxLen && i < dependents.length - 1) {
        showNames.push('...')
        break
      }
      showNames.push(name)
    }
    const show = `(${showNames.join(', ')})`
    messages.push(`${dependents.length - max} more ${show}`)
  }

  const str = '\n' + messages.join('\n')
  return str.split('\n').join('\n  ')
}

const explainEdge = ({ name, type, from, spec }, depth, color) => {
  const { bold } = color ? chalk : nocolor
  return (type === 'prod' ? '' : `${colorType(type, color)} `) +
    `${bold(name)}@"${bold(spec)}" from ` +
    explainFrom(from, depth, color)
}

const explainFrom = (from, depth, color) => {
  if (!from.name && !from.version)
    return 'the root project'

  return printNode(from, color) +
    explainDependents(from, depth - 1, color)
}

module.exports = { explainNode, printNode, explainEdge }
