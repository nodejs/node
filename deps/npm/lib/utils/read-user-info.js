'use strict'
const Bluebird = require('bluebird')
const readAsync = Bluebird.promisify(require('read'))
const userValidate = require('npm-user-validate')
const log = require('npmlog')

exports.otp = readOTP
exports.password = readPassword
exports.username = readUsername
exports.email = readEmail

function read (opts) {
  return Bluebird.try(() => {
    log.clearProgress()
    return readAsync(opts)
  }).finally(() => {
    log.showProgress()
  })
}

function readOTP (msg, otp, isRetry) {
  if (!msg) {
    msg = [
      'This command requires a one-time password (OTP) from your authenticator app.',
      'Enter one below. You can also pass one on the command line by appending --otp=123456.',
      'For more information, see:',
      'https://docs.npmjs.com/getting-started/using-two-factor-authentication',
      'Enter OTP: '
    ].join('\n')
  }
  if (isRetry && otp && /^[\d ]+$|^[A-Fa-f0-9]{64,64}$/.test(otp)) return otp.replace(/\s+/g, '')

  return read({prompt: msg, default: otp || ''})
    .then((otp) => readOTP(msg, otp, true))
}

function readPassword (msg, password, isRetry) {
  if (!msg) msg = 'npm password: '
  if (isRetry && password) return password

  return read({prompt: msg, silent: true, default: password || ''})
    .then((password) => readPassword(msg, password, true))
}

function readUsername (msg, username, opts, isRetry) {
  if (!msg) msg = 'npm username: '
  if (isRetry && username) {
    const error = userValidate.username(username)
    if (error) {
      opts.log && opts.log.warn(error.message)
    } else {
      return Promise.resolve(username.trim())
    }
  }

  return read({prompt: msg, default: username || ''})
    .then((username) => readUsername(msg, username, opts, true))
}

function readEmail (msg, email, opts, isRetry) {
  if (!msg) msg = 'email (this IS public): '
  if (isRetry && email) {
    const error = userValidate.email(email)
    if (error) {
      opts.log && opts.log.warn(error.message)
    } else {
      return email.trim()
    }
  }

  return read({prompt: msg, default: email || ''})
    .then((username) => readEmail(msg, username, opts, true))
}
