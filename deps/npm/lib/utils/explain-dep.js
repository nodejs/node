const chalk = require('chalk')
const nocolor = {
  bold: s => s,
  dim: s => s,
  red: s => s,
  yellow: s => s,
  cyan: s => s,
  magenta: s => s,
  blue: s => s,
  green: s => s,
}

const { relative } = require('path')

const explainNode = (node, depth, color) =>
  printNode(node, color) +
  explainDependents(node, depth, color) +
  explainLinksIn(node, depth, color)

const colorType = (type, color) => {
  const { red, yellow, cyan, magenta, blue, green } = color ? chalk : nocolor
  const style = type === 'extraneous' ? red
    : type === 'dev' ? yellow
    : type === 'optional' ? cyan
    : type === 'peer' ? magenta
    : type === 'bundled' ? blue
    : type === 'workspace' ? green
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
    bundled,
    isWorkspace,
  } = node
  const { bold, dim, green } = color ? chalk : nocolor
  const extra = []
  if (extraneous) {
    extra.push(' ' + bold(colorType('extraneous', color)))
  }

  if (dev) {
    extra.push(' ' + bold(colorType('dev', color)))
  }

  if (optional) {
    extra.push(' ' + bold(colorType('optional', color)))
  }

  if (peer) {
    extra.push(' ' + bold(colorType('peer', color)))
  }

  if (bundled) {
    extra.push(' ' + bold(colorType('bundled', color)))
  }

  const pkgid = isWorkspace
    ? green(`${name}@${version}`)
    : `${bold(name)}@${bold(version)}`

  return `${pkgid}${extra.join('')}` +
    (location ? dim(`\n${location}`) : '')
}

const explainLinksIn = ({ linksIn }, depth, color) => {
  if (!linksIn || !linksIn.length || depth <= 0) {
    return ''
  }

  const messages = linksIn.map(link => explainNode(link, depth - 1, color))
  const str = '\n' + messages.join('\n')
  return str.split('\n').join('\n  ')
}

const explainDependents = ({ name, dependents }, depth, color) => {
  if (!dependents || !dependents.length || depth <= 0) {
    return ''
  }

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

const explainEdge = ({ name, type, bundled, from, spec }, depth, color) => {
  const { bold } = color ? chalk : nocolor
  const dep = type === 'workspace'
    ? bold(relative(from.location, spec.slice('file:'.length)))
    : `${bold(name)}@"${bold(spec)}"`
  const fromMsg = ` from ${explainFrom(from, depth, color)}`

  return (type === 'prod' ? '' : `${colorType(type, color)} `) +
    (bundled ? `${colorType('bundled', color)} ` : '') +
    `${dep}${fromMsg}`
}

const explainFrom = (from, depth, color) => {
  if (!from.name && !from.version) {
    return 'the root project'
  }

  return printNode(from, color) +
    explainDependents(from, depth - 1, color) +
    explainLinksIn(from, depth - 1, color)
}

module.exports = { explainNode, printNode, explainEdge }
