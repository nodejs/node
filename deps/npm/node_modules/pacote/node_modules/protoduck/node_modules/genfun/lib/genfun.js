'use strict'

const Method = require('./method')
const Role = require('./role')
const util = require('./util')

const kCache = Symbol('cache')
const kDefaultMethod = Symbol('defaultMethod')
const kMethods = Symbol('methods')
const kNoNext = Symbol('noNext')

module.exports = function genfun (opts) {
  function gf () {
    if (!gf[kMethods].length && gf[kDefaultMethod]) {
      return gf[kDefaultMethod].func.apply(this, arguments)
    } else {
      return gf.applyGenfun(this, arguments)
    }
  }
  Object.setPrototypeOf(gf, Genfun.prototype)
  gf[kMethods] = []
  gf[kCache] = {key: [], methods: [], state: STATES.UNINITIALIZED}
  if (opts && typeof opts === 'function') {
    gf.add(opts)
  } else if (opts && opts.default) {
    gf.add(opts.default)
  }
  if (opts && opts.name) {
    Object.defineProperty(gf, 'name', {
      value: opts.name
    })
  }
  if (opts && opts.noNextMethod) {
    gf[kNoNext] = true
  }
  return gf
}

class Genfun extends Function {}
Genfun.prototype.isGenfun = true

const STATES = {
  UNINITIALIZED: 0,
  MONOMORPHIC: 1,
  POLYMORPHIC: 2,
  MEGAMORPHIC: 3
}

const MAX_CACHE_SIZE = 32

/**
 * Defines a method on a generic function.
 *
 * @function
 * @param {Array-like} selector - Selector array for dispatching the method.
 * @param {Function} methodFunction - Function to execute when the method
 *                                    successfully dispatches.
 */
Genfun.prototype.add = function addMethod (selector, func) {
  if (!func && typeof selector === 'function') {
    func = selector
    selector = []
  }
  selector = [].slice.call(selector)
  for (var i = 0; i < selector.length; i++) {
    if (!selector.hasOwnProperty(i)) {
      selector[i] = Object.prototype
    }
  }
  this[kCache] = {key: [], methods: [], state: STATES.UNINITIALIZED}
  let method = new Method(this, selector, func)
  if (selector.length) {
    this[kMethods].push(method)
  } else {
    this[kDefaultMethod] = method
  }
  return this
}

/**
 * Removes a previously-defined method on `genfun` that matches
 * `selector` exactly.
 *
 * @function
 * @param {Genfun} genfun - Genfun to remove a method from.
 * @param {Array-like} selector - Objects to match on when finding a
 *                                    method to remove.
 */
Genfun.prototype.rm = function removeMethod () {
  throw new Error('not yet implemented')
}

/**
 * Returns true if there are methods that apply to the given arguments on
 * `genfun`. Additionally, makes sure the cache is warmed up for the given
 * arguments.
 *
 */
Genfun.prototype.hasMethod = function hasMethod () {
  const methods = this.getApplicableMethods(arguments)
  return !!(methods && methods.length)
}

/**
 * This generic function is called when `genfun` has been called and no
 * applicable method was found. The default method throws an `Error`.
 *
 * @function
 * @param {Genfun} genfun - Generic function instance that was called.
 * @param {*} newthis - value of `this` the genfun was called with.
 * @param {Array} callArgs - Arguments the genfun was called with.
 */
module.exports.noApplicableMethod = module.exports()
module.exports.noApplicableMethod.add([], (gf, thisArg, args) => {
  let msg =
        'No applicable method found when called with arguments of types: (' +
        [].map.call(args, (arg) => {
          return (/\[object ([a-zA-Z0-9]+)\]/)
            .exec(({}).toString.call(arg))[1]
        }).join(', ') + ')'
  let err = new Error(msg)
  err.genfun = gf
  err.thisArg = thisArg
  err.args = args
  throw err
})

/*
 * Internal
 */
Genfun.prototype.applyGenfun = function applyGenfun (newThis, args) {
  let applicableMethods = this.getApplicableMethods(args)
  if (applicableMethods.length === 1 || this[kNoNext]) {
    return applicableMethods[0].func.apply(newThis, args)
  } else if (applicableMethods.length > 1) {
    let idx = 0
    const nextMethod = function nextMethod () {
      if (arguments.length) {
        // Replace args if passed in explicitly
        args = arguments
        Array.prototype.push.call(args, nextMethod)
      }
      const next = applicableMethods[idx++]
      if (idx >= applicableMethods.length) {
        Array.prototype.pop.call(args)
      }
      return next.func.apply(newThis, args)
    }
    Array.prototype.push.call(args, nextMethod)
    return nextMethod()
  } else {
    return module.exports.noApplicableMethod(this, newThis, args)
  }
}

Genfun.prototype.getApplicableMethods = function getApplicableMethods (args) {
  if (!args.length || !this[kMethods].length) {
    return this[kDefaultMethod] ? [this[kDefaultMethod]] : []
  }
  let applicableMethods
  let maybeMethods = cachedMethods(this, args)
  if (maybeMethods) {
    applicableMethods = maybeMethods
  } else {
    applicableMethods = computeApplicableMethods(this, args)
    cacheArgs(this, args, applicableMethods)
  }
  return applicableMethods
}

function cacheArgs (genfun, args, methods) {
  if (genfun[kCache].state === STATES.MEGAMORPHIC) { return }
  var key = []
  var proto
  for (var i = 0; i < args.length; i++) {
    proto = cacheableProto(genfun, args[i])
    if (proto) {
      key[i] = proto
    } else {
      return null
    }
  }
  genfun[kCache].key.unshift(key)
  genfun[kCache].methods.unshift(methods)
  if (genfun[kCache].key.length === 1) {
    genfun[kCache].state = STATES.MONOMORPHIC
  } else if (genfun[kCache].key.length < MAX_CACHE_SIZE) {
    genfun[kCache].state = STATES.POLYMORPHIC
  } else {
    genfun[kCache].state = STATES.MEGAMORPHIC
  }
}

function cacheableProto (genfun, arg) {
  var dispatchable = util.dispatchableObject(arg)
  if (Object.hasOwnProperty.call(dispatchable, Role.roleKeyName)) {
    for (var j = 0; j < dispatchable[Role.roleKeyName].length; j++) {
      var role = dispatchable[Role.roleKeyName][j]
      if (role.method.genfun === genfun) {
        return null
      }
    }
  }
  return Object.getPrototypeOf(dispatchable)
}

function cachedMethods (genfun, args) {
  if (genfun[kCache].state === STATES.UNINITIALIZED ||
      genfun[kCache].state === STATES.MEGAMORPHIC) {
    return null
  }
  var protos = []
  var proto
  for (var i = 0; i < args.length; i++) {
    proto = cacheableProto(genfun, args[i])
    if (proto) {
      protos[i] = proto
    } else {
      return
    }
  }
  for (i = 0; i < genfun[kCache].key.length; i++) {
    if (matchCachedMethods(genfun[kCache].key[i], protos)) {
      return genfun[kCache].methods[i]
    }
  }
}

function matchCachedMethods (key, protos) {
  if (key.length !== protos.length) { return false }
  for (var i = 0; i < key.length; i++) {
    if (key[i] !== protos[i]) {
      return false
    }
  }
  return true
}

function computeApplicableMethods (genfun, args) {
  args = [].slice.call(args)
  let discoveredMethods = []
  function findAndRankRoles (object, hierarchyPosition, index) {
    var roles = Object.hasOwnProperty.call(object, Role.roleKeyName)
    ? object[Role.roleKeyName]
    : []
    roles.forEach(role => {
      if (role.method.genfun === genfun && index === role.position) {
        if (discoveredMethods.indexOf(role.method) < 0) {
          Method.clearRank(role.method)
          discoveredMethods.push(role.method)
        }
        Method.setRankHierarchyPosition(role.method, index, hierarchyPosition)
      }
    })
    // When a discovered method would receive more arguments than
    // were specialized, we pretend all extra arguments have a role
    // on Object.prototype.
    if (util.isObjectProto(object)) {
      discoveredMethods.forEach(method => {
        if (method.minimalSelector <= index) {
          Method.setRankHierarchyPosition(method, index, hierarchyPosition)
        }
      })
    }
  }
  args.forEach((arg, index) => {
    getPrecedenceList(util.dispatchableObject(arg))
      .forEach((obj, hierarchyPosition) => {
        findAndRankRoles(obj, hierarchyPosition, index)
      })
  })
  let applicableMethods = discoveredMethods.filter(method => {
    return (args.length === method._rank.length &&
            Method.isFullySpecified(method))
  })
  applicableMethods.sort((a, b) => Method.score(a) - Method.score(b))
  if (genfun[kDefaultMethod]) {
    applicableMethods.push(genfun[kDefaultMethod])
  }
  return applicableMethods
}

/*
 * Helper function for getting an array representing the entire
 * inheritance/precedence chain for an object by navigating its
 * prototype pointers.
 */
function getPrecedenceList (obj) {
  var precedenceList = []
  var nextObj = obj
  while (nextObj) {
    precedenceList.push(nextObj)
    nextObj = Object.getPrototypeOf(nextObj)
  }
  return precedenceList
}
