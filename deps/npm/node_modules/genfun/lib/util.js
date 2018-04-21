'use strict'

module.exports.isObjectProto = isObjectProto
function isObjectProto (obj) {
  return obj === Object.prototype
}

const _null = {}
const _undefined = {}
const Bool = Boolean
const Num = Number
const Str = String
const boolCache = {
  true: new Bool(true),
  false: new Bool(false)
}
const numCache = {}
const strCache = {}

/*
 * Returns a useful dispatch object for value using a process similar to
 * the ToObject operation specified in http://es5.github.com/#x9.9
 */
module.exports.dispatchableObject = dispatchableObject
function dispatchableObject (value) {
  // To shut up jshint, which doesn't let me turn off this warning.
  const Obj = Object
  if (value === null) { return _null }
  if (value === undefined) { return _undefined }
  switch (typeof value) {
    case 'object': return value
    case 'boolean': return boolCache[value]
    case 'number': return numCache[value] || (numCache[value] = new Num(value))
    case 'string': return strCache[value] || (strCache[value] = new Str(value))
    default: return new Obj(value)
  }
}
