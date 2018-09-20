'use strict'

const log = require('npmlog')

const deprecated = {}
const deprWarned = {}

module.exports = deprCheck
function deprCheck (data) {
  if (deprecated[data._id]) {
    data.deprecated = deprecated[data._id]
  }

  if (data.deprecated) {
    deprecated[data._id] = data.deprecated
    if (!deprWarned[data._id]) {
      deprWarned[data._id] = true
      log.warn('deprecated', '%s: %s', data._id, data.deprecated)
    }
  }

  return data
}
