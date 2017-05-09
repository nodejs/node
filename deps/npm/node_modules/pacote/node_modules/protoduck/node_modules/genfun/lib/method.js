'use strict'

/*
 * Method
 *
 * Methods are added, conceptually, to Genfuns, not to objects
 * themselves, although the Genfun object does not have any pointers to
 * method objects.
 *
 * The _rank vector is an internal datastructure used during dispatch
 * to figure out whether a method is applicable, and if so, how to
 * order multiple discovered methods.
 *
 * Right now, the score method on Method does not take into account any
 * ordering, and all arguments to a method are ranked equally for the
 * sake of ordering.
 *
 */
const Role = require('./role')
const util = require('./util')

module.exports = Method
function Method (genfun, selector, func) {
  var method = this
  method.genfun = genfun
  method.func = func
  method._rank = []
  method.minimalSelector = 0

  const tmpSelector = selector.length ? selector : [Object.prototype]
  for (var object, i = tmpSelector.length - 1; i >= 0; i--) {
    object = Object.hasOwnProperty.call(tmpSelector, i)
    ? tmpSelector[i]
    : Object.prototype
    object = util.dispatchableObject(object)
    if (
      typeof object === 'function' &&
      !object.isGenfun
    ) {
      object = object.prototype
    }
    if (i > 0 &&
        !method.minimalSelector &&
        util.isObjectProto(object)) {
      continue
    } else {
      method.minimalSelector++
      if (!Object.hasOwnProperty.call(object, Role.roleKeyName)) {
        if (Object.defineProperty) {
          // Object.defineProperty is JS 1.8.0+
          Object.defineProperty(
            object, Role.roleKeyName, {value: [], enumerable: false})
        } else {
          object[Role.roleKeyName] = []
        }
      }
      // XXX HACK - no method replacement now, so we just shove
      // it in a place where it'll always show up first. This
      // would probably seriously break method combination if we
      // had it.
      object[Role.roleKeyName].unshift(new Role(method, i))
    }
  }
}

Method.setRankHierarchyPosition = (method, index, hierarchyPosition) => {
  method._rank[index] = hierarchyPosition
}

Method.clearRank = method => {
  method._rank = []
}

Method.isFullySpecified = method => {
  for (var i = 0; i < method.minimalSelector; i++) {
    if (!method._rank.hasOwnProperty(i)) {
      return false
    }
  }
  return true
}

Method.score = method => {
  // TODO - this makes all items in the list equal
  return method._rank.reduce((a, b) => a + b, 0)
}
