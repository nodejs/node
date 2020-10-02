'use strict'

const reporters = {
  install: require('./reporters/install'),
  detail: require('./reporters/detail'),
  json: require('./reporters/json'),
  quiet: require('./reporters/quiet')
}

const exitCode = require('./exit-code.js')

module.exports = Object.assign((data, options = {}) => {
  const {
    reporter = 'install',
    color = true,
    unicode = true,
    indent = 2,
    auditLevel = 'low'
  } = options

  if (!data)
    throw Object.assign(
      new TypeError('ENOAUDITDATA'),
      {
        code: 'ENOAUDITDATA',
        message: 'missing audit data'
      }
    )

  if (typeof data.toJSON === 'function')
    data = data.toJSON()

  return {
    report: reporters[reporter](data, { color, unicode, indent }),
    exitCode: exitCode(data, auditLevel)
  }
}, { reporters })
