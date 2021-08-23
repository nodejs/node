// An edge in the dependency graph
// Represents a dependency relationship of some kind

const util = require('util')
const npa = require('npm-package-arg')
const depValid = require('./dep-valid.js')
const _from = Symbol('_from')
const _to = Symbol('_to')
const _type = Symbol('_type')
const _spec = Symbol('_spec')
const _accept = Symbol('_accept')
const _name = Symbol('_name')
const _error = Symbol('_error')
const _loadError = Symbol('_loadError')
const _setFrom = Symbol('_setFrom')
const _explain = Symbol('_explain')
const _explanation = Symbol('_explanation')

const types = new Set([
  'prod',
  'dev',
  'optional',
  'peer',
  'peerOptional',
  'workspace',
])

class ArboristEdge {}
const printableEdge = (edge) => {
  const edgeFrom = edge.from && edge.from.location
  const edgeTo = edge.to && edge.to.location

  return Object.assign(new ArboristEdge(), {
    name: edge.name,
    spec: edge.spec,
    type: edge.type,
    ...(edgeFrom != null ? { from: edgeFrom } : {}),
    ...(edgeTo ? { to: edgeTo } : {}),
    ...(edge.error ? { error: edge.error } : {}),
    ...(edge.overridden ? { overridden: true } : {}),
  })
}

class Edge {
  constructor (options) {
    const { type, name, spec, accept, from } = options

    if (typeof spec !== 'string')
      throw new TypeError('must provide string spec')

    if (type === 'workspace' && npa(spec).type !== 'directory')
      throw new TypeError('workspace edges must be a symlink')

    this[_spec] = spec

    if (accept !== undefined) {
      if (typeof accept !== 'string')
        throw new TypeError('accept field must be a string if provided')
      this[_accept] = accept || '*'
    }

    if (typeof name !== 'string')
      throw new TypeError('must provide dependency name')
    this[_name] = name

    if (!types.has(type)) {
      throw new TypeError(
        `invalid type: ${type}\n` +
        `(valid types are: ${Edge.types.join(', ')})`)
    }
    this[_type] = type
    if (!from)
      throw new TypeError('must provide "from" node')
    this[_setFrom](from)
    this[_error] = this[_loadError]()
    this.overridden = false
  }

  satisfiedBy (node) {
    return node.name === this.name && depValid(node, this.spec, this.accept, this.from)
  }

  explain (seen = []) {
    if (this[_explanation])
      return this[_explanation]

    return this[_explanation] = this[_explain](seen)
  }

  // return the edge data, and an explanation of how that edge came to be here
  [_explain] (seen) {
    const { error, from, bundled } = this
    return {
      type: this.type,
      name: this.name,
      spec: this.spec,
      ...(bundled ? { bundled } : {}),
      ...(error ? { error } : {}),
      ...(from ? { from: from.explain(null, seen) } : {}),
    }
  }

  get bundled () {
    if (!this.from)
      return false
    const { package: { bundleDependencies = [] } } = this.from
    return bundleDependencies.includes(this.name)
  }

  get workspace () {
    return this[_type] === 'workspace'
  }

  get prod () {
    return this[_type] === 'prod'
  }

  get dev () {
    return this[_type] === 'dev'
  }

  get optional () {
    return this[_type] === 'optional' || this[_type] === 'peerOptional'
  }

  get peer () {
    return this[_type] === 'peer' || this[_type] === 'peerOptional'
  }

  get type () {
    return this[_type]
  }

  get name () {
    return this[_name]
  }

  get spec () {
    return this[_spec]
  }

  get accept () {
    return this[_accept]
  }

  get valid () {
    return !this.error
  }

  get missing () {
    return this.error === 'MISSING'
  }

  get invalid () {
    return this.error === 'INVALID'
  }

  get peerLocal () {
    return this.error === 'PEER LOCAL'
  }

  get error () {
    this[_error] = this[_error] || this[_loadError]()
    return this[_error] === 'OK' ? null : this[_error]
  }

  [_loadError] () {
    return !this[_to] ? (this.optional ? null : 'MISSING')
      : this.peer && this.from === this.to.parent && !this.from.isTop ? 'PEER LOCAL'
      : !this.satisfiedBy(this.to) ? 'INVALID'
      : 'OK'
  }

  reload (hard = false) {
    this[_explanation] = null
    const newTo = this[_from].resolve(this.name)
    if (newTo !== this[_to]) {
      if (this[_to])
        this[_to].edgesIn.delete(this)
      this[_to] = newTo
      this[_error] = this[_loadError]()
      if (this[_to])
        this[_to].addEdgeIn(this)
    } else if (hard)
      this[_error] = this[_loadError]()
  }

  detach () {
    this[_explanation] = null
    if (this[_to])
      this[_to].edgesIn.delete(this)
    this[_from].edgesOut.delete(this.name)
    this[_to] = null
    this[_error] = 'DETACHED'
    this[_from] = null
  }

  [_setFrom] (node) {
    this[_explanation] = null
    this[_from] = node
    if (node.edgesOut.has(this.name))
      node.edgesOut.get(this.name).detach()
    node.addEdgeOut(this)
    this.reload()
  }

  get from () {
    return this[_from]
  }

  get to () {
    return this[_to]
  }

  toJSON () {
    return printableEdge(this)
  }

  [util.inspect.custom] () {
    return this.toJSON()
  }
}

Edge.types = [...types]
Edge.errors = [
  'DETACHED',
  'MISSING',
  'PEER LOCAL',
  'INVALID',
]

module.exports = Edge
