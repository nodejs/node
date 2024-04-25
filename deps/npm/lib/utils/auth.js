const profile = require('npm-profile')
const { log } = require('proc-log')
const openUrlPrompt = require('../utils/open-url-prompt.js')
const read = require('../utils/read-user-info.js')
const otplease = require('../utils/otplease.js')

const adduser = async (npm, { creds, ...opts }) => {
  const authType = npm.config.get('auth-type')
  let res
  if (authType === 'web') {
    try {
      res = await profile.adduserWeb((url, emitter) => {
        openUrlPrompt(
          npm,
          url,
          'Create your account at',
          'Press ENTER to open in the browser...',
          emitter
        )
      }, opts)
    } catch (err) {
      if (err.code === 'ENYI') {
        log.verbose('web add user not supported, trying couch')
      } else {
        throw err
      }
    }
  }

  // auth type !== web or ENYI error w/ web adduser
  if (!res) {
    const username = await read.username('Username:', creds.username)
    const password = await read.password('Password:', creds.password)
    const email = await read.email('Email: (this IS public) ', creds.email)
    // npm registry quirk: If you "add" an existing user with their current
    // password, it's effectively a login, and if that account has otp you'll
    // be prompted for it.
    res = await otplease(npm, opts, (reqOpts) =>
      profile.adduserCouch(username, email, password, opts)
    )
  }

  // We don't know the username if it was a web login, all we can reliably log is scope and registry
  const message = `Logged in${opts.scope ? ` to scope ${opts.scope}` : ''} on ${opts.registry}.`

  log.info('adduser', message)

  return {
    message,
    newCreds: { token: res.token },
  }
}

const login = async (npm, { creds, ...opts }) => {
  const authType = npm.config.get('auth-type')
  let res
  if (authType === 'web') {
    try {
      res = await profile.loginWeb((url, emitter) => {
        openUrlPrompt(
          npm,
          url,
          'Login at',
          'Press ENTER to open in the browser...',
          emitter
        )
      }, opts)
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
    res = await otplease(npm, opts, (reqOpts) =>
      profile.loginCouch(username, password, reqOpts)
    )
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
  adduser,
  login,
}
