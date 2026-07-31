const { webAuthOpener, loginWeb, loginCouch } = require('npm-profile')
const { log } = require('proc-log')
const { createOpener } = require('../utils/open-url.js')
const read = require('../utils/read-user-info.js')

const otplease = async (npm, opts, fn) => {
  try {
    return await fn(opts)
  } catch (err) {
    if (!process.stdin.isTTY || !process.stdout.isTTY) {
      throw err
    }

    // web otp
    if (err.code === 'EOTP' && err.body?.authUrl && err.body?.doneUrl) {
      const { token: otp } = await webAuthOpener(
        createOpener(npm, 'Authenticate your account at'),
        err.body.authUrl,
        err.body.doneUrl,
        opts
      )
      return await fn({ ...opts, otp })
    }

    // classic otp
    if (err.code === 'EOTP' || (err.code === 'E401' && /one-time pass/.test(err.body))) {
      const otp = await read.otp('This operation requires a one-time password.\nEnter OTP:')
      return await fn({ ...opts, otp })
    }

    throw err
  }
}

const login = async (npm, { creds, ...opts }) => {
  const authType = npm.config.get('auth-type')
  let res
  if (authType === 'web') {
    try {
      res = await loginWeb(createOpener(npm, 'Login at'), opts)
    } catch (err) {
      if (err.code === 'ENYI') {
        log.verbose('web login not supported, trying couch')
      } else {
        throw err
      }
    }
  }

  // auth type !== web or ENYI error w/ web login
  if (!res) {
    const username = await read.username('Username:', creds.username)
    const password = await read.password('Password:', creds.password)
    res = await otplease(npm, opts, (reqOpts) => loginCouch(username, password, reqOpts))
  }

  // We don't know the username if it was a web login, all we can reliably log is scope and registry
  const message = `Logged in${opts.scope ? ` to scope ${opts.scope}` : ''} on ${opts.registry}.`

  log.info('login', message)

  return {
    message,
    newCreds: { token: res.token },
  }
}

module.exports = {
  login,
  otplease,
}
