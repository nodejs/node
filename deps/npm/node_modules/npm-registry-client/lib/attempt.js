var retry = require("retry")

module.exports = attempt

function attempt(cb) {
  // Tuned to spread 3 attempts over about a minute.
  // See formula at <https://github.com/tim-kos/node-retry>.
  var operation = retry.operation(this.config.retry)

  var client = this
  operation.attempt(function (currentAttempt) {
    client.log.info("attempt", "registry request try #"+currentAttempt+
                    " at "+(new Date()).toLocaleTimeString())

    cb(operation)
  })
}
