var log = require('npmlog')

var deprecated = {}
var deprWarned = {}
module.exports = function deprCheck (data) {
  if (deprecated[data._id]) data.deprecated = deprecated[data._id]
  if (data.deprecated) deprecated[data._id] = data.deprecated
  else return
  if (!deprWarned[data._id]) {
    deprWarned[data._id] = true
    log.warn('deprecated', '%s: %s', data._id, data.deprecated)
  }
}
