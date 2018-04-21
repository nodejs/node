'use strict'

const report = function (data, options) {
  let total = 0

  const keys = Object.keys(data.metadata.vulnerabilities)
  for (let key of keys) {
    const value = data.metadata.vulnerabilities[key]
    total = total + value
  }

  return {
    report: '',
    exitCode: total === 0 ? 0 : 1
  }
}

module.exports = report
