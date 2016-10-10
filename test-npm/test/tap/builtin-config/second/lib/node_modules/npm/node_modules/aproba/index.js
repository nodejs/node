'use strict'

function isArguments (thingy) {
  return typeof thingy === 'object' && thingy.hasOwnProperty('callee')
}

var types = {
  '*': ['any', function () { return true }],
  A: ['array', function (thingy) { return Array.isArray(thingy) || isArguments(thingy) }],
  S: ['string', function (thingy) { return typeof thingy === 'string' }],
  N: ['number', function (thingy) { return typeof thingy === 'number' }],
  F: ['function', function (thingy) { return typeof thingy === 'function' }],
  O: ['object', function (thingy) { return typeof thingy === 'object' && !types.A[1](thingy) && !types.E[1](thingy) }],
  B: ['boolean', function (thingy) { return typeof thingy === 'boolean' }],
  E: ['error', function (thingy) { return thingy instanceof Error }]
}

var validate = module.exports = function (schema, args) {
  if (!schema) throw missingRequiredArg(0, 'schema')
  if (!args) throw missingRequiredArg(1, 'args')
  if (!types.S[1](schema)) throw invalidType(0, 'string', schema)
  if (!types.A[1](args)) throw invalidType(1, 'array', args)
  for (var ii = 0; ii < schema.length; ++ii) {
    var type = schema[ii]
    if (!types[type]) throw unknownType(ii, type)
    var typeLabel = types[type][0]
    var typeCheck = types[type][1]
    if (type === 'E' && args[ii] == null) continue
    if (args[ii] == null) throw missingRequiredArg(ii)
    if (!typeCheck(args[ii])) throw invalidType(ii, typeLabel, args[ii])
    if (type === 'E') return
  }
  if (schema.length < args.length) throw tooManyArgs(schema.length, args.length)
}

function missingRequiredArg (num) {
  return newException('EMISSINGARG', 'Missing required argument #' + (num + 1))
}

function unknownType (num, type) {
  return newException('EUNKNOWNTYPE', 'Unknown type ' + type + ' in argument #' + (num + 1))
}

function invalidType (num, expectedType, value) {
  var valueType
  Object.keys(types).forEach(function (typeCode) {
    if (types[typeCode][1](value)) valueType = types[typeCode][0]
  })
  return newException('EINVALIDTYPE', 'Argument #' + (num + 1) + ': Expected ' +
    expectedType + ' but got ' + valueType)
}

function tooManyArgs (expected, got) {
  return newException('ETOOMANYARGS', 'Too many arguments, expected ' + expected + ' and got ' + got)
}

function newException (code, msg) {
  var e = new Error(msg)
  e.code = code
  Error.captureStackTrace(e, validate)
  return e
}
