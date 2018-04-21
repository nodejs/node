'use strict'

const report = function (data, options) {
  const defaults = {
    indent: 2
  }

  const config = Object.assign({}, defaults, options)

  const json = JSON.stringify(data, null, config.indent)
  return {
    report: json,
    exitCode: 0
  }
}

module.exports = report
