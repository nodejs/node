module.exports = warnDeprecated

var log = require("npmlog")

var deprecations = {}

function warnDeprecated (type) {
  return function warn (messages, instance) {
    if (!instance) {
      if (!deprecations[type]) {
        deprecations[type] = {}
        messages.forEach(function (m) { log.warn(type, m) })
      }
    }
    else {
      if (!deprecations[type]) deprecations[type] = {}

      if (!deprecations[type][instance]) {
        deprecations[type][instance] = true
        messages.forEach(function (m) { log.warn(type, m) })
      }
    }
  }
}
