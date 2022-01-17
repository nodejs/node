const { promisify } = require('util')
const readAsync = promisify(require('read'))
const userValidate = require('npm-user-validate')
const log = require('./log-shim.js')

exports.otp = readOTP
exports.password = readPassword
exports.username = readUsername
exports.email = readEmail

const otpPrompt = `This command requires a one-time password (OTP) from your authenticator app.
Enter one below. You can also pass one on the command line by appending --otp=123456.
For more information, see:
https://docs.npmjs.com/getting-started/using-two-factor-authentication
Enter OTP: `
const passwordPrompt = 'npm password: '
const usernamePrompt = 'npm username: '
const emailPrompt = 'email (this IS public): '

function read (opts) {
  log.clearProgress()
  return readAsync(opts).finally(() => log.showProgress())
}

function readOTP (msg = otpPrompt, otp, isRetry) {
  if (isRetry && otp && /^[\d ]+$|^[A-Fa-f0-9]{64,64}$/.test(otp)) {
    return otp.replace(/\s+/g, '')
  }

  return read({ prompt: msg, default: otp || '' })
    .then((otp) => readOTP(msg, otp, true))
}

function readPassword (msg = passwordPrompt, password, isRetry) {
  if (isRetry && password) {
    return password
  }

  return read({ prompt: msg, silent: true, default: password || '' })
    .then((password) => readPassword(msg, password, true))
}

function readUsername (msg = usernamePrompt, username, isRetry) {
  if (isRetry && username) {
    const error = userValidate.username(username)
    if (error) {
      log.warn(error.message)
    } else {
      return Promise.resolve(username.trim())
    }
  }

  return read({ prompt: msg, default: username || '' })
    .then((username) => readUsername(msg, username, true))
}

function readEmail (msg = emailPrompt, email, isRetry) {
  if (isRetry && email) {
    const error = userValidate.email(email)
    if (error) {
      log.warn(error.message)
    } else {
      return email.trim()
    }
  }

  return read({ prompt: msg, default: email || '' })
    .then((username) => readEmail(msg, username, true))
}
