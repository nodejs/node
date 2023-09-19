const { relative } = require('path')

const explainNode = (node, depth, chalk) =>
  printNode(node, chalk) +
  explainDependents(node, depth, chalk) +
  explainLinksIn(node, depth, chalk)

const colorType = (type, chalk) => {
  const { red, yellow, cyan, magenta, blue, green, gray } = chalk
  const style = type === 'extraneous' ? red
    : type === 'dev' ? yellow
    : type === 'optional' ? cyan
    : type === 'peer' ? magenta
    : type === 'bundled' ? blue
    : type === 'workspace' ? green
    : type === 'overridden' ? gray
    : /* istanbul ignore next */ s => s
  return style(type)
}

const printNode = (node, chalk) => {
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
    overridden,
  } = node
  const { bold, dim, green } = chalk
  const extra = []
  if (extraneous) {
    extra.push(' ' + bold(colorType('extraneous', chalk)))
  }

  if (dev) {
    extra.push(' ' + bold(colorType('dev', chalk)))
  }

  if (optional) {
    extra.push(' ' + bold(colorType('optional', chalk)))
  }

  if (peer) {
    extra.push(' ' + bold(colorType('peer', chalk)))
  }

  if (bundled) {
    extra.push(' ' + bold(colorType('bundled', chalk)))
  }

  if (overridden) {
    extra.push(' ' + bold(colorType('overridden', chalk)))
  }

  const pkgid = isWorkspace
    ? green(`${name}@${version}`)
    : `${bold(name)}@${bold(version)}`

  return `${pkgid}${extra.join('')}` +
    (location ? dim(`\n${location}`) : '')
}

const explainLinksIn = ({ linksIn }, depth, chalk) => {
  if (!linksIn || !linksIn.length || depth <= 0) {
    return ''
  }

  const messages = linksIn.map(link => explainNode(link, depth - 1, chalk))
  const str = '\n' + messages.join('\n')
  return str.split('\n').join('\n  ')
}

const explainDependents = ({ name, dependents }, depth, chalk) => {
  if (!dependents || !dependents.length || depth <= 0) {
    return ''
  }

  const max = Math.ceil(depth / 2)
  const messages = dependents.slice(0, max)
    .map(edge => explainEdge(edge, depth, chalk))

  // show just the names of the first 5 deps that overflowed the list
  if (dependents.length > max) {
    let len = 0
    const maxLen = 50
    const showNames = []
    for (let i = max; i < dependents.length; i++) {
      const { from: { name: depName = 'the root project' } } = dependents[i]
      len += depName.length
      if (len >= maxLen && i < dependents.length - 1) {
        showNames.push('...')
        break
      }
      showNames.push(depName)
    }
    const show = `(${showNames.join(', ')})`
    messages.push(`${dependents.length - max} more ${show}`)
  }

  const str = '\n' + messages.join('\n')
  return str.split('\n').join('\n  ')
}

const explainEdge = ({ name, type, bundled, from, spec, rawSpec, overridden }, depth, chalk) => {
  const { bold } = chalk
  let dep = type === 'workspace'
    ? bold(relative(from.location, spec.slice('file:'.length)))
    : `${bold(name)}@"${bold(spec)}"`
  if (overridden) {
    dep = `${colorType('overridden', chalk)} ${dep} (was "${rawSpec}")`
  }

  const fromMsg = ` from ${explainFrom(from, depth, chalk)}`

  return (type === 'prod' ? '' : `${colorType(type, chalk)} `) +
    (bundled ? `${colorType('bundled', chalk)} ` : '') +
    `${dep}${fromMsg}`
}

const explainFrom = (from, depth, chalk) => {
  if (!from.name && !from.version) {
    return 'the root project'
  }

  return printNode(from, chalk) +
    explainDependents(from, depth - 1, chalk) +
    explainLinksIn(from, depth - 1, chalk)
}

module.exports = { explainNode, printNode, explainEdge }
