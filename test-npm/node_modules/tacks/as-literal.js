'use strict'
module.exports = asLiteral

var asObjectKey = require('./as-object-key.js')

function asLiteral (thing) {
  if (thing === null) return 'null'
  if (thing == null) return 'undefined'
  if (typeof thing === 'boolean' || typeof thing === 'number') {
    return thing.toString()
  } else if (typeof thing === 'string') {
    return asStringLiteral(thing)
  } else if (thing instanceof Array) {
    return asArrayLiteral(thing)
  } else {
    return asObjectLiteral(thing)
  }
}

function asStringLiteral (thing) {
  var str = thing.toString()
    .replace(/\\/g, '\\\\')
    .replace(/[\0]/g, '\\0')
    .replace(/[\b]/g, '\\b')
    .replace(/[\f]/g, '\\f')
    .replace(/[\n]/g, '\\n')
    .replace(/[\r]/g, '\\r')
    .replace(/[\t]/g, '\\t')
    .replace(/[\v]/g, '\\v')
  if (/'/.test(str) && !/"/.test(str)) {
    return '"' + str + '"'
  } else {
    return "'" + str.replace(/'/g, "\\'") + "'"
  }
}

function asArrayLiteral (thing) {
  if (!thing.length) return '[ ]'
  var arr = '[\n'
  function arrayItem (item) {
    return asLiteral(item).replace(/\n(.*)(?=\n)/g, '\n  $1')
  }
  arr += arrayItem(thing.shift())
  thing.forEach(function (item) {
    arr += ',\n' + arrayItem(item)
  })
  arr += '\n]'
  return arr
}


function asObjectLiteral (thing) {
  var keys = Object.keys(thing)
  if (!keys.length) return '{ }'
  var obj = '{\n'
  function objectValue (key) {
    return asObjectKey(key) + ': ' + asLiteral(thing[key]).replace(/\n(.*)(?=\n)/g, '\n  $1')
  }
  obj += objectValue(keys.shift())
  keys.forEach(function (key) {
    obj += ',\n' + objectValue(key)
  })
  obj += '\n}'
  return obj
}
