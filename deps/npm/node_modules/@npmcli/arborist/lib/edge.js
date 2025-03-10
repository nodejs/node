// An edge in the dependency graph
// Represents a dependency relationship of some kind

const util = require('node:util')
const npa = require('npm-package-arg')
const depValid = require('./dep-valid.js')
const OverrideSet = require('./override-set.js')

class ArboristEdge {
  constructor (edge) {
    this.name = edge.name
    this.spec = edge.spec
    this.type = edge.type

    const edgeFrom = edge.from?.location
    const edgeTo = edge.to?.location
    const override = edge.overrides?.value

    if (edgeFrom != null) {
      this.from = edgeFrom
    }
    if (edgeTo) {
      this.to = edgeTo
    }
    if (edge.error) {
      this.error = edge.error
    }
    if (edge.peerConflicted) {
      this.peerConflicted = true
    }
    if (override) {
      this.overridden = override
    }
  }
}

class Edge {
  #accept
  #error
  #explanation
  #from
  #name
  #spec
  #to
  #type

  static types = Object.freeze([
    'prod',
    'dev',
    'optional',
    'peer',
    'peerOptional',
    'workspace',
  ])

  // XXX where is this used?
  static errors = Object.freeze([
    'DETACHED',
    'MISSING',
    'PEER LOCAL',
    'INVALID',
  ])

  constructor (options) {
    const { type, name, spec, accept, from, overrides } = options

    // XXX are all of these error states even possible?
    if (typeof spec !== 'string') {
      throw new TypeError('must provide string spec')
    }
    if (!Edge.types.includes(type)) {
      throw new TypeError(`invalid type: ${type}\n(valid types are: ${Edge.types.join(', ')})`)
    }
    if (type === 'workspace' && npa(spec).type !== 'directory') {
      throw new TypeError('workspace edges must be a symlink')
    }
    if (typeof name !== 'string') {
      throw new TypeError('must provide dependency name')
    }
    if (!from) {
      throw new TypeError('must provide "from" node')
    }
    if (accept !== undefined) {
      if (typeof accept !== 'string') {
        throw new TypeError('accept field must be a string if provided')
      }
      this.#accept = accept || '*'
    }
    if (overrides !== undefined) {
      this.overrides = overrides
    }

    this.#name = name
    this.#type = type
    this.#spec = spec
    this.#explanation = null
    this.#from = from

    from.edgesOut.get(this.#name)?.detach()
    from.addEdgeOut(this)

    this.reload(true)
    this.peerConflicted = false
  }

  satisfiedBy (node) {
    if (node.name !== this.#name || !this.#from) {
      return false
    }

    // NOTE: this condition means we explicitly do not support overriding
    // bundled or shrinkwrapped dependencies
    if (node.hasShrinkwrap || node.inShrinkwrap || node.inBundle) {
      return depValid(node, this.rawSpec, this.#accept, this.#from)
    }

    // If there's no override we just use the spec.
    if (!this.overrides?.keySpec) {
      return depValid(node, this.spec, this.#accept, this.#from)
    }
    // There's some override. If the target node satisfies the overriding spec
    // then it's okay.
    if (depValid(node, this.spec, this.#accept, this.#from)) {
      return true
    }
    // If it doesn't, then it should at least satisfy the original spec.
    if (!depValid(node, this.rawSpec, this.#accept, this.#from)) {
      return false
    }
    // It satisfies the original spec, not the overriding spec. We need to make
    // sure it doesn't use the overridden spec.
    // For example:
    //   we might have an ^8.0.0 rawSpec, and an override that makes
    //   keySpec=8.23.0 and the override value spec=9.0.0.
    //   If the node is 9.0.0, then it's okay because it's consistent with spec.
    //   If the node is 8.24.0, then it's okay because it's consistent with the rawSpec.
    //   If the node is 8.23.0, then it's not okay because even though it's consistent
    //   with the rawSpec, it's also consistent with the keySpec.
    //   So we're looking for ^8.0.0 or 9.0.0 and not 8.23.0.
    return !depValid(node, this.overrides.keySpec, this.#accept, this.#from)
  }

  // return the edge data, and an explanation of how that edge came to be here
  explain (seen = []) {
    if (!this.#explanation) {
      const explanation = {
        type: this.#type,
        name: this.#name,
        spec: this.spec,
      }
      if (this.rawSpec !== this.spec) {
        explanation.rawSpec = this.rawSpec
        explanation.overridden = true
      }
      if (this.bundled) {
        explanation.bundled = this.bundled
      }
      if (this.error) {
        explanation.error = this.error
      }
      if (this.#from) {
        explanation.from = this.#from.explain(null, seen)
      }
      this.#explanation = explanation
    }
    return this.#explanation
  }

  get bundled () {
    return !!this.#from?.package?.bundleDependencies?.includes(this.#name)
  }

  get workspace () {
    return this.#type === 'workspace'
  }

  get prod () {
    return this.#type === 'prod'
  }

  get dev () {
    return this.#type === 'dev'
  }

  get optional () {
    return this.#type === 'optional' || this.#type === 'peerOptional'
  }

  get peer () {
    return this.#type === 'peer' || this.#type === 'peerOptional'
  }

  get type () {
    return this.#type
  }

  get name () {
    return this.#name
  }

  get rawSpec () {
    return this.#spec
  }

  get spec () {
    if (this.overrides?.value && this.overrides.value !== '*' && this.overrides.name === this.#name) {
      if (this.overrides.value.startsWith('$')) {
        const ref = this.overrides.value.slice(1)
        const pkg = this.#from?.sourceReference
          ? this.#from?.sourceReference.root.package
          : this.#from?.root?.package
        if (pkg.devDependencies?.[ref]) {
          return pkg.devDependencies[ref]
        }
        if (pkg.optionalDependencies?.[ref]) {
          return pkg.optionalDependencies[ref]
        }
        if (pkg.dependencies?.[ref]) {
          return pkg.dependencies[ref]
        }
        if (pkg.peerDependencies?.[ref]) {
          return pkg.peerDependencies[ref]
        }

        throw new Error(`Unable to resolve reference ${this.overrides.value}`)
      }
      return this.overrides.value
    }
    return this.#spec
  }

  get accept () {
    return this.#accept
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
    if (!this.#error) {
      if (!this.#to) {
        if (this.optional) {
          this.#error = null
        } else {
          this.#error = 'MISSING'
        }
      } else if (this.peer && this.#from === this.#to.parent && !this.#from?.isTop) {
        this.#error = 'PEER LOCAL'
      } else if (!this.satisfiedBy(this.#to)) {
        this.#error = 'INVALID'
      } else if (this.overrides && this.#to.edgesOut.size && OverrideSet.doOverrideSetsConflict(this.overrides, this.#to.overrides)) {
        // Any inconsistency between the edge's override set and the target's override set is potentially problematic.
        // But we only say the edge is in error if the override sets are plainly conflicting.
        // Note that if the target doesn't have any dependencies of their own, then this inconsistency is irrelevant.
        this.#error = 'INVALID'
      } else {
        this.#error = 'OK'
      }
    }
    if (this.#error === 'OK') {
      return null
    }
    return this.#error
  }

  reload (hard = false) {
    this.#explanation = null

    let needToUpdateOverrideSet = false
    let newOverrideSet
    let oldOverrideSet
    if (this.#from?.overrides) {
      newOverrideSet = this.#from.overrides.getEdgeRule(this)
      if (newOverrideSet && !newOverrideSet.isEqual(this.overrides)) {
        // If there's a new different override set we need to propagate it to the nodes.
        // If we're deleting the override set then there's no point propagating it right now since it will be filled with another value later.
        needToUpdateOverrideSet = true
        oldOverrideSet = this.overrides
        this.overrides = newOverrideSet
      }
    } else {
      delete this.overrides
    }
    const newTo = this.#from?.resolve(this.#name)
    if (newTo !== this.#to) {
      if (this.#to) {
        this.#to.deleteEdgeIn(this)
      }
      this.#to = newTo
      this.#error = null
      if (this.#to) {
        this.#to.addEdgeIn(this)
      }
    } else if (hard) {
      this.#error = null
    } else if (needToUpdateOverrideSet && this.#to) {
      // Propagate the new override set to the target node.
      this.#to.updateOverridesEdgeInRemoved(oldOverrideSet)
      this.#to.updateOverridesEdgeInAdded(newOverrideSet)
    }
  }

  detach () {
    this.#explanation = null
    if (this.#to) {
      this.#to.deleteEdgeIn(this)
    }
    this.#from?.edgesOut.delete(this.#name)
    this.#to = null
    this.#error = 'DETACHED'
    this.#from = null
  }

  get from () {
    return this.#from
  }

  get to () {
    return this.#to
  }

  toJSON () {
    return new ArboristEdge(this)
  }

  [util.inspect.custom] () {
    return this.toJSON()
  }
}

module.exports = Edge
