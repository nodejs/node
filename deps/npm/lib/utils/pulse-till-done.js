const validate = require('aproba')
const log = require('npmlog')

let pulsers = 0
let pulse

function pulseStart (prefix) {
  if (++pulsers > 1)
    return
  pulse = setInterval(function () {
    log.gauge.pulse(prefix)
  }, 150)
}
function pulseStop () {
  if (--pulsers > 0)
    return
  clearInterval(pulse)
}

module.exports = function (prefix, cb) {
  validate('SF', [prefix, cb])
  if (!prefix)
    prefix = 'network'
  pulseStart(prefix)
  return (er, ...args) => {
    pulseStop()
    cb(er, ...args)
  }
}

const pulseWhile = async (prefix, promise) => {
  if (!promise) {
    promise = prefix
    prefix = ''
  }
  pulseStart(prefix)
  try {
    return await promise
  } finally {
    pulseStop()
  }
}
module.exports.withPromise = pulseWhile
