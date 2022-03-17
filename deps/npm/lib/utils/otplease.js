const readUserInfo = require('./read-user-info.js')

module.exports = otplease
async function otplease (opts, fn) {
  try {
    await fn(opts)
  } catch (err) {
    if (err.code !== 'EOTP' && (err.code !== 'E401' || !/one-time pass/.test(err.body))) {
      throw err
    } else if (!process.stdin.isTTY || !process.stdout.isTTY) {
      throw err
    } else {
      const otp = await readUserInfo.otp('This operation requires a one-time password.\nEnter OTP:')
      return fn({ ...opts, otp })
    }
  }
}
