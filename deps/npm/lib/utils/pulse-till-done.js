const log = require('./log-shim.js')

let pulseTimer = null
const withPromise = async (promise) => {
  pulseStart()
  try {
    return await promise
  } finally {
    pulseStop()
  }
}

const pulseStart = () => {
  pulseTimer = pulseTimer || setInterval(() => {
    log.gauge.pulse('')
  }, 150)
}

const pulseStop = () => {
  clearInterval(pulseTimer)
  pulseTimer = null
}

module.exports = {
  withPromise,
}
