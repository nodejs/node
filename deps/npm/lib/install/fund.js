'use strict'

const { EOL } = require('os')

const computeMetadata = require('./deps.js').computeMetadata
const mutateIntoLogicalTree = require('./mutate-into-logical-tree.js')
var { getFundingInfo } = require('../utils/funding.js')

exports.getPrintFundingReport = getPrintFundingReport
exports.getPrintFundingReportJSON = getPrintFundingReportJSON

function getFundingResult ({ fund, idealTree }) {
  if (fund) {
    const fundingInfoTree =
      mutateIntoLogicalTree.asReadInstalled(
        computeMetadata(idealTree)
      )
    const fundResult = getFundingInfo(fundingInfoTree, { countOnly: true })
    return fundResult
  } else {
    return {}
  }
}

function getPrintFundingReport ({ fund, idealTree }, opts) {
  const fundResult = getFundingResult({ fund, idealTree })
  const { length } = fundResult || {}
  const { json } = opts || {}

  function padding (msg) {
    return json ? '' : (EOL + msg)
  }

  function packageQuantity (amount) {
    return `package${amount > 1 ? 's are' : ' is'}`
  }

  if (!length) return ''

  return padding('') + length + ' ' +
    packageQuantity(length) +
    ' looking for funding' +
    padding('  run `npm fund` for details\n')
}

function getPrintFundingReportJSON ({ fund, idealTree }) {
  return getPrintFundingReport({ fund, idealTree }, { json: true })
}
