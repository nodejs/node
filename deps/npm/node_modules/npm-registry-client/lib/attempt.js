var retry = require("retry")

module.exports = attempt

function attempt(cb) {
  // Tuned to spread 3 attempts over about a minute.
  // See formula at <https://github.com/tim-kos/node-retry>.
  var operation = retry.operation({
    retries    : this.conf.get("fetch-retries") || 2,
    factor     : this.conf.get("fetch-retry-factor"),
    minTimeout : this.conf.get("fetch-retry-mintimeout") || 10000,
    maxTimeout : this.conf.get("fetch-retry-maxtimeout") || 60000
  })

  var client = this
  operation.attempt(function (currentAttempt) {
    client.log.info("attempt", "registry request try #"+currentAttempt+
                    " at "+(new Date()).toLocaleTimeString())

    cb(operation)
  })
}
