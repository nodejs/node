'use strict'

const Utils = require('../lib/utils')

const report = function (data) {
  const totalVulnCount = Utils.totalVulnCount(data.metadata.vulnerabilities)

  return {
    report: '',
    exitCode: totalVulnCount === 0 ? 0 : 1
  }
}

module.exports = report
