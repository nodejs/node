'use strict'

const reporters = {
  install: require('./reporters/install'),
  parseable: require('./reporters/parseable'),
  detail: require('./reporters/detail'),
  json: require('./reporters/json'),
  quiet: require('./reporters/quiet')
}

const report = function (data, options) {
  const defaults = {
    reporter: 'install',
    withColor: true,
    withUnicode: true
  }

  const config = Object.assign({}, defaults, options)
  return new Promise((resolve) => {
    const result = reporters[config.reporter](data, config)
    return resolve(result)
  })
}

module.exports = report
