const prompt = 'This operation requires a one-time password.\nEnter OTP:'
const readUserInfo = require('./read-user-info.js')

const isOtpError = err =>
  err.code === 'EOTP' || (err.code === 'E401' && /one-time pass/.test(err.body))

module.exports = otplease
function otplease (opts, fn) {
  opts = { prompt, ...opts }
  return Promise.resolve().then(() => fn(opts)).catch(err => {
    if (!isOtpError(err)) {
      throw err
    } else if (!process.stdin.isTTY || !process.stdout.isTTY) {
      throw err
    } else {
      return readUserInfo.otp(opts.prompt)
        .then(otp => fn({ ...opts, otp }))
    }
  })
}
