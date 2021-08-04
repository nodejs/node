const log = require('npmlog')
const profile = require('npm-profile')

const openUrl = require('../utils/open-url.js')
const read = require('../utils/read-user-info.js')

const loginPrompter = async (creds) => {
  const opts = { log: log }

  creds.username = await read.username('Username:', creds.username, opts)
  creds.password = await read.password('Password:', creds.password)
  creds.email = await read.email('Email: (this IS public) ', creds.email, opts)

  return creds
}

const login = async (npm, opts) => {
  let res

  const requestOTP = async () => {
    const otp = await read.otp(
      'Enter one-time password from your authenticator app: '
    )

    return profile.loginCouch(
      opts.creds.username,
      opts.creds.password,
      { ...opts, otp }
    )
  }

  const addNewUser = async () => {
    let newUser

    try {
      newUser = await profile.adduserCouch(
        opts.creds.username,
        opts.creds.email,
        opts.creds.password,
        opts
      )
    } catch (err) {
      if (err.code === 'EOTP')
        newUser = await requestOTP()
      else
        throw err
    }

    return newUser
  }

  const openerPromise = (url) => openUrl(npm, url, 'to complete your login please visit')
  try {
    res = await profile.login(openerPromise, loginPrompter, opts)
  } catch (err) {
    const needsMoreInfo = !(opts &&
      opts.creds &&
      opts.creds.username &&
      opts.creds.password &&
      opts.creds.email)
    if (err.code === 'EOTP')
      res = await requestOTP()
    else if (needsMoreInfo)
      throw err
    else {
      // TODO: maybe this needs to check for err.code === 'E400' instead?
      res = await addNewUser()
    }
  }

  const newCreds = {}
  if (res && res.token)
    newCreds.token = res.token
  else {
    newCreds.username = opts.creds.username
    newCreds.password = opts.creds.password
    newCreds.email = opts.creds.email
    newCreds.alwaysAuth = opts.creds.alwaysAuth
  }

  const usermsg = opts.creds.username ? ` user ${opts.creds.username}` : ''
  const scopeMessage = opts.scope ? ` to scope ${opts.scope}` : ''
  const userout = opts.creds.username ? ` as ${opts.creds.username}` : ''
  const message = `Logged in${userout}${scopeMessage} on ${opts.registry}.`

  log.info('login', `Authorized${usermsg}`)

  return {
    message,
    newCreds,
  }
}

module.exports = login
