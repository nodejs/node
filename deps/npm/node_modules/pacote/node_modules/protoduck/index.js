'use strict'

const genfun = require('genfun')

class Duck extends Function {
  // Duck.impl(Foo, [String, Array], { frob (str, arr) { ... }})
  impl (target, types, impls) {
    if (!impls && !isArray(types)) {
      impls = types
      types = []
    }
    if (!impls && this.isDerivable) {
      impls = this._defaultImpls
    }
    if (!impls) {
      impls = {}
    }
    if (typeof target === 'function' && !target.isGenfun) {
      target = target.prototype
    }
    checkImpls(this, target, impls)
    checkArgTypes(this, types)
    this._constraints.forEach(c => {
      if (!c.verify(target, types)) {
        throw new Error(`Implementations of ${
          this.name || 'this protocol'
        } must first implement ${
          c.parent.name || 'its constraint protocols defined in opts.where.'
        }`)
      }
    })
    this._methodNames.forEach(name => {
      defineMethod(this, name, target, types, impls)
    })
  }

  hasImpl (arg, args) {
    args = args || []
    const fns = this._methodNames
    var gf
    if (typeof arg === 'function' && !arg.isGenfun) {
      arg = arg.prototype
    }
    args = args.map(arg => {
      if (typeof arg === 'function' && !arg.isGenfun) {
        return arg.prototype
      } else {
        return arg
      }
    })
    for (var i = 0; i < fns.length; i++) {
      gf = arg[fns[i]]
      if (!gf ||
          (gf.hasMethod
          ? !gf.hasMethod.apply(gf, args)
          : typeof gf === 'function')) {
        return false
      }
    }
    return true
  }

  // MyDuck.matches('a', ['this', 'c'])
  matches (thisType, argTypes) {
    if (!argTypes && isArray(thisType)) {
      argTypes = thisType
      thisType = 'this'
    }
    if (!thisType) {
      thisType = 'this'
    }
    if (!argTypes) {
      argTypes = []
    }
    return new Constraint(this, thisType, argTypes)
  }
}
Duck.prototype.isDuck = true
Duck.prototype.isProtocol = true

const Protoduck = module.exports = define(['duck'], {
  createGenfun: ['duck', _metaCreateGenfun],
  addMethod: ['duck', _metaAddMethod]
}, {name: 'Protoduck'})

const noImplFound = module.exports.noImplFound = genfun.noApplicableMethod

module.exports.define = define
function define (types, spec, opts) {
  if (!isArray(types)) {
    // protocol(spec, opts?) syntax for method-based protocols
    opts = spec
    spec = types
    types = []
  }
  const duck = function (thisType, argTypes) {
    return duck.matches(thisType, argTypes)
  }
  Object.setPrototypeOf(duck, Duck.prototype)
  duck.isDerivable = true
  Object.defineProperty(duck, 'name', {
    value: (opts && opts.name) || 'Protocol'
  })
  if (opts && opts.where) {
    let where = opts.where
    if (!isArray(opts.where)) { where = [opts.where] }
    duck._constraints = where.map(w => w.isProtocol // `where: [Foo]`
      ? w.matches()
      : w
    )
  } else {
    duck._constraints = []
  }
  duck.isProtocol = true
  duck._metaobject = opts && opts.metaobject
  duck._types = types
  duck._defaultImpls = {}
  duck._gfTypes = {}
  duck._methodNames = Object.keys(spec)
  duck._methodNames.forEach(name => {
    checkMethodSpec(duck, name, spec)
  })
  duck._constraints.forEach(c => c.attach(duck))
  return duck
}

function checkMethodSpec (duck, name, spec) {
  let gfTypes = spec[name]
  if (typeof gfTypes === 'function') {
    duck._defaultImpls[name] = gfTypes
    gfTypes = [gfTypes]
  } if (typeof gfTypes[gfTypes.length - 1] === 'function') {
    duck._defaultImpls[name] = gfTypes.pop()
  } else {
    duck.isDerivable = false
  }
  duck._gfTypes[name] = gfTypes.map(typeId => {
    const idx = duck._types.indexOf(typeId)
    if (idx === -1) {
      throw new Error(
        `type '${
          typeId
        }' for function '${
          name
        }' does not match any protocol types (${
          duck._types.join(', ')
        }).`
      )
    } else {
      return idx
    }
  })
}

function defineMethod (duck, name, target, types, impls) {
  const methodTypes = duck._gfTypes[name].map(function (typeIdx) {
    return types[typeIdx]
  })
  for (let i = methodTypes.length - 1; i >= 0; i--) {
    if (methodTypes[i] === undefined) {
      methodTypes.pop()
    } else {
      break
    }
  }
  const useMetaobject = duck._metaobject && duck._metaobject !== Protoduck
  // `target` does not necessarily inherit from `Object`
  if (!Object.prototype.hasOwnProperty.call(target, name)) {
    // Make a genfun if there's nothing there
    const gf = useMetaobject
    ? duck._metaobject.createGenfun(duck, target, name, null)
    : _metaCreateGenfun(duck, target, name, null)
    target[name] = gf
  } else if (typeof target[name] === 'function' && !target[name].isGenfun) {
    // Turn non-gf functions into genfuns
    const gf = useMetaobject
    ? duck._metaobject.createGenfun(duck, target, name, target[name])
    : _metaCreateGenfun(duck, target, name, target[name])
    target[name] = gf
  }

  const fn = impls[name] || duck._defaultImpls[name]
  if (fn) { // checkImpls made sure this is safe
    useMetaobject
    ? duck._metaobject.addMethod(duck, target, name, methodTypes, fn)
    : _metaAddMethod(duck, target, name, methodTypes, fn)
  }
}

function checkImpls (duck, target, impls) {
  duck._methodNames.forEach(function (name) {
    if (
      !impls[name] &&
      !duck._defaultImpls[name] &&
      // Existing methods on the target are acceptable defaults.
      typeof target[name] !== 'function'
    ) {
      throw new Error(`Missing implementation for ${
        formatMethod(duck, name, duck.name)
      }. Make sure the method is present in your ${
        duck.name || 'protocol'
      } definition. Required methods: ${
        duck._methodNames.filter(m => {
          return !duck._defaultImpls[m]
        }).map(m => formatMethod(duck, m)).join(', ')
      }.`)
    }
  })
  Object.keys(impls).forEach(function (name) {
    if (duck._methodNames.indexOf(name) === -1) {
      throw new Error(
        `${name}() was included in the impl, but is not part of ${
          duck.name || 'the protocol'
        }. Allowed methods: ${
          duck._methodNames.map(m => formatMethod(duck, m)).join(', ')
        }.`
      )
    }
  })
}

function formatMethod (duck, name, withDuckName) {
  return `${
    withDuckName && duck.name ? `${duck.name}#` : ''
  }${name}(${duck._gfTypes[name].map(n => duck._types[n]).join(', ')})`
}

function checkArgTypes (duck, types) {
  var requiredTypes = duck._types
  if (types.length > requiredTypes.length) {
    throw new Error(
      `${
        duck.name || 'Protocol'
      } expects to be defined across ${
        requiredTypes.length
      } type${requiredTypes.length > 1 ? 's' : ''}, but ${
        types.length
      } ${types.length > 1 ? 'were' : 'was'} specified.`
    )
  }
}

function typeName (obj) {
  return (/\[object ([a-zA-Z0-9]+)\]/).exec(({}).toString.call(obj))[1]
}

function installMethodErrorMessage (proto, gf, target, name) {
  noImplFound.add([gf], function (gf, thisArg, args) {
    let parent = Object.getPrototypeOf(thisArg)
    while (parent && parent[name] === gf) {
      parent = Object.getPrototypeOf(parent)
    }
    if (parent && parent[name] && typeof parent[name] === 'function') {
    }
    var msg = `No ${typeName(thisArg)} impl for ${
      proto.name ? `${proto.name}#` : ''
    }${name}(${[].map.call(args, typeName).join(', ')}). You must implement ${
      proto.name
      ? formatMethod(proto, name, true)
      : `the protocol ${formatMethod(proto, name)} belongs to`
    } in order to call ${typeName(thisArg)}#${name}(${
      [].map.call(args, typeName).join(', ')
    }).`
    const err = new Error(msg)
    err.protocol = proto
    err.function = gf
    err.thisArg = thisArg
    err.args = args
    err.code = 'ENOIMPL'
    throw err
  })
}

function isArray (x) {
  return Object.prototype.toString.call(x) === '[object Array]'
}

// Metaobject Protocol
Protoduck.impl(Protoduck) // defaults configured by definition

function _metaCreateGenfun (proto, target, name, deflt) {
  var gf = genfun({
    default: deflt,
    name: `${proto.name ? `${proto.name}#` : ''}${name}`
  })
  installMethodErrorMessage(proto, gf, target, name)
  gf.duck = proto
  return gf
}

function _metaAddMethod (duck, target, name, methodTypes, fn) {
  return target[name].add(methodTypes, fn)
}

// Constraints
class Constraint {
  constructor (parent, thisType, argTypes) {
    this.parent = parent
    this.target = thisType
    this.types = argTypes
  }

  attach (obj) {
    this.child = obj
    if (this.target === 'this') {
      this.thisIdx = 'this'
    } else {
      const idx = this.child._types.indexOf(this.target)
      if (idx === -1) {
        this.thisIdx = null
      } else {
        this.thisIdx = idx
      }
    }
    this.indices = this.types.map(typeId => {
      if (typeId === 'this') {
        return 'this'
      } else {
        const idx = this.child._types.indexOf(typeId)
        if (idx === -1) {
          return null
        } else {
          return idx
        }
      }
    })
  }

  verify (target, types) {
    const thisType = (
      this.thisIdx === 'this' || this.thisIdx == null
    )
    ? target
    : types[this.thisIdx]
    const parentTypes = this.indices.map(idx => {
      if (idx === 'this') {
        return target
      } else if (idx === 'this') {
        return types[this.thisIdx]
      } else if (idx === null) {
        return Object
      } else {
        return types[idx] || Object.prototype
      }
    })
    return this.parent.hasImpl(thisType, parentTypes)
  }
}
Constraint.prototype.isConstraint = true
