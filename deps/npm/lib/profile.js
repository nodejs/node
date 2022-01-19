'use strict'

const BB = require('bluebird')

const ansistyles = require('ansistyles')
const figgyPudding = require('figgy-pudding')
const inspect = require('util').inspect
const log = require('npmlog')
const npm = require('./npm.js')
const npmConfig = require('./config/figgy-config.js')
const otplease = require('./utils/otplease.js')
const output = require('./utils/output.js')
const profile = require('libnpm/profile')
const pulseTillDone = require('./utils/pulse-till-done.js')
const qrcodeTerminal = require('qrcode-terminal')
const queryString = require('query-string')
const qw = require('qw')
const readUserInfo = require('./utils/read-user-info.js')
const Table = require('cli-table3')
const url = require('url')

module.exports = profileCmd

profileCmd.usage =
  'npm profile enable-2fa [auth-only|auth-and-writes]\n' +
  'npm profile disable-2fa\n' +
  'npm profile get [<key>]\n' +
  'npm profile set <key> <value>'

profileCmd.subcommands = qw`enable-2fa disable-2fa get set`

profileCmd.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  switch (argv[2]) {
    case 'enable-2fa':
    case 'enable-tfa':
      if (argv.length === 3) {
        return cb(null, qw`auth-and-writes auth-only`)
      } else {
        return cb(null, [])
      }
    case 'disable-2fa':
    case 'disable-tfa':
    case 'get':
    case 'set':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

function withCb (prom, cb) {
  prom.then((value) => cb(null, value), cb)
}

const ProfileOpts = figgyPudding({
  json: {},
  otp: {},
  parseable: {},
  registry: {}
})

function profileCmd (args, cb) {
  if (args.length === 0) return cb(new Error(profileCmd.usage))
  log.gauge.show('profile')
  switch (args[0]) {
    case 'enable-2fa':
    case 'enable-tfa':
    case 'enable2fa':
    case 'enabletfa':
      withCb(enable2fa(args.slice(1)), cb)
      break
    case 'disable-2fa':
    case 'disable-tfa':
    case 'disable2fa':
    case 'disabletfa':
      withCb(disable2fa(), cb)
      break
    case 'get':
      withCb(get(args.slice(1)), cb)
      break
    case 'set':
      withCb(set(args.slice(1)), cb)
      break
    default:
      cb(new Error('Unknown profile command: ' + args[0]))
  }
}

const knownProfileKeys = qw`
  name email ${'two-factor auth'} fullname homepage
  freenode twitter github created updated`

function get (args) {
  const tfa = 'two-factor auth'
  const conf = ProfileOpts(npmConfig())
  return pulseTillDone.withPromise(profile.get(conf)).then((info) => {
    if (!info.cidr_whitelist) delete info.cidr_whitelist
    if (conf.json) {
      output(JSON.stringify(info, null, 2))
      return
    }
    const cleaned = {}
    knownProfileKeys.forEach((k) => { cleaned[k] = info[k] || '' })
    Object.keys(info).filter((k) => !(k in cleaned)).forEach((k) => { cleaned[k] = info[k] || '' })
    delete cleaned.tfa
    delete cleaned.email_verified
    cleaned['email'] += info.email_verified ? ' (verified)' : '(unverified)'
    if (info.tfa && !info.tfa.pending) {
      cleaned[tfa] = info.tfa.mode
    } else {
      cleaned[tfa] = 'disabled'
    }
    if (args.length) {
      const values = args // comma or space separated ↓
        .join(',').split(/,/).map((arg) => arg.trim()).filter((arg) => arg !== '')
        .map((arg) => cleaned[arg])
        .join('\t')
      output(values)
    } else {
      if (conf.parseable) {
        Object.keys(info).forEach((key) => {
          if (key === 'tfa') {
            output(`${key}\t${cleaned[tfa]}`)
          } else {
            output(`${key}\t${info[key]}`)
          }
        })
      } else {
        const table = new Table()
        Object.keys(cleaned).forEach((k) => table.push({[ansistyles.bright(k)]: cleaned[k]}))
        output(table.toString())
      }
    }
  })
}

const writableProfileKeys = qw`
  email password fullname homepage freenode twitter github`

function set (args) {
  let conf = ProfileOpts(npmConfig())
  const prop = (args[0] || '').toLowerCase().trim()
  let value = args.length > 1 ? args.slice(1).join(' ') : null
  if (prop !== 'password' && value === null) {
    return Promise.reject(Error('npm profile set <prop> <value>'))
  }
  if (prop === 'password' && value !== null) {
    return Promise.reject(Error(
      'npm profile set password\n' +
      'Do not include your current or new passwords on the command line.'))
  }
  if (writableProfileKeys.indexOf(prop) === -1) {
    return Promise.reject(Error(`"${prop}" is not a property we can set. Valid properties are: ` + writableProfileKeys.join(', ')))
  }
  return BB.try(() => {
    if (prop === 'password') {
      return readUserInfo.password('Current password: ').then((current) => {
        return readPasswords().then((newpassword) => {
          value = {old: current, new: newpassword}
        })
      })
    } else if (prop === 'email') {
      return readUserInfo.password('Password: ').then((current) => {
        return {password: current, email: value}
      })
    }
    function readPasswords () {
      return readUserInfo.password('New password: ').then((password1) => {
        return readUserInfo.password('       Again:     ').then((password2) => {
          if (password1 !== password2) {
            log.warn('profile', 'Passwords do not match, please try again.')
            return readPasswords()
          }
          return password1
        })
      })
    }
  }).then(() => {
    // FIXME: Work around to not clear everything other than what we're setting
    return pulseTillDone.withPromise(profile.get(conf).then((user) => {
      const newUser = {}
      writableProfileKeys.forEach((k) => { newUser[k] = user[k] })
      newUser[prop] = value
      return otplease(conf, conf => profile.set(newUser, conf))
        .then((result) => {
          if (conf.json) {
            output(JSON.stringify({[prop]: result[prop]}, null, 2))
          } else if (conf.parseable) {
            output(prop + '\t' + result[prop])
          } else if (result[prop] != null) {
            output('Set', prop, 'to', result[prop])
          } else {
            output('Set', prop)
          }
        })
    }))
  })
}

function enable2fa (args) {
  if (args.length > 1) {
    return Promise.reject(new Error('npm profile enable-2fa [auth-and-writes|auth-only]'))
  }
  const mode = args[0] || 'auth-and-writes'
  if (mode !== 'auth-only' && mode !== 'auth-and-writes') {
    return Promise.reject(new Error(`Invalid two-factor authentication mode "${mode}".\n` +
      'Valid modes are:\n' +
      '  auth-only - Require two-factor authentication only when logging in\n' +
      '  auth-and-writes - Require two-factor authentication when logging in AND when publishing'))
  }
  const conf = ProfileOpts(npmConfig())
  if (conf.json || conf.parseable) {
    return Promise.reject(new Error(
      'Enabling two-factor authentication is an interactive operation and ' +
      (conf.json ? 'JSON' : 'parseable') + ' output mode is not available'))
  }

  const info = {
    tfa: {
      mode: mode
    }
  }

  return BB.try(() => {
    // if they're using legacy auth currently then we have to update them to a
    // bearer token before continuing.
    const auth = getAuth(conf)
    if (auth.basic) {
      log.info('profile', 'Updating authentication to bearer token')
      return profile.createToken(
        auth.basic.password, false, [], conf
      ).then((result) => {
        if (!result.token) throw new Error('Your registry ' + conf.registry + 'does not seem to support bearer tokens. Bearer tokens are required for two-factor authentication')
        npm.config.setCredentialsByURI(conf.registry, {token: result.token})
        return BB.fromNode((cb) => npm.config.save('user', cb))
      })
    }
  }).then(() => {
    log.notice('profile', 'Enabling two factor authentication for ' + mode)
    return readUserInfo.password()
  }).then((password) => {
    info.tfa.password = password
    log.info('profile', 'Determine if tfa is pending')
    return pulseTillDone.withPromise(profile.get(conf)).then((info) => {
      if (!info.tfa) return
      if (info.tfa.pending) {
        log.info('profile', 'Resetting two-factor authentication')
        return pulseTillDone.withPromise(profile.set({tfa: {password, mode: 'disable'}}, conf))
      } else {
        if (conf.auth.otp) return
        return readUserInfo.otp('Enter one-time password: ').then((otp) => {
          conf.auth.otp = otp
        })
      }
    })
  }).then(() => {
    log.info('profile', 'Setting two-factor authentication to ' + mode)
    return pulseTillDone.withPromise(profile.set(info, conf))
  }).then((challenge) => {
    if (challenge.tfa === null) {
      output('Two factor authentication mode changed to: ' + mode)
      return
    }
    if (typeof challenge.tfa !== 'string' || !/^otpauth:[/][/]/.test(challenge.tfa)) {
      throw new Error('Unknown error enabling two-factor authentication. Expected otpauth URL, got: ' + inspect(challenge.tfa))
    }
    const otpauth = url.parse(challenge.tfa)
    const opts = queryString.parse(otpauth.query)
    return qrcode(challenge.tfa).then((code) => {
      output('Scan into your authenticator app:\n' + code + '\n Or enter code:', opts.secret)
    }).then((code) => {
      return readUserInfo.otp('And an OTP code from your authenticator: ')
    }).then((otp1) => {
      log.info('profile', 'Finalizing two-factor authentication')
      return profile.set({tfa: [otp1]}, conf)
    }).then((result) => {
      output('2FA successfully enabled. Below are your recovery codes, please print these out.')
      output('You will need these to recover access to your account if you lose your authentication device.')
      result.tfa.forEach((c) => output('\t' + c))
    })
  })
}

function getAuth (conf) {
  const creds = npm.config.getCredentialsByURI(conf.registry)
  let auth
  if (creds.token) {
    auth = {token: creds.token}
  } else if (creds.username) {
    auth = {basic: {username: creds.username, password: creds.password}}
  } else if (creds.auth) {
    const basic = Buffer.from(creds.auth, 'base64').toString().split(':', 2)
    auth = {basic: {username: basic[0], password: basic[1]}}
  } else {
    auth = {}
  }

  if (conf.otp) auth.otp = conf.otp
  return auth
}

function disable2fa (args) {
  let conf = ProfileOpts(npmConfig())
  return pulseTillDone.withPromise(profile.get(conf)).then((info) => {
    if (!info.tfa || info.tfa.pending) {
      output('Two factor authentication not enabled.')
      return
    }
    return readUserInfo.password().then((password) => {
      return BB.try(() => {
        if (conf.otp) return
        return readUserInfo.otp('Enter one-time password: ').then((otp) => {
          conf = conf.concat({otp})
        })
      }).then(() => {
        log.info('profile', 'disabling tfa')
        return pulseTillDone.withPromise(profile.set({tfa: {password: password, mode: 'disable'}}, conf)).then(() => {
          if (conf.json) {
            output(JSON.stringify({tfa: false}, null, 2))
          } else if (conf.parseable) {
            output('tfa\tfalse')
          } else {
            output('Two factor authentication disabled.')
          }
        })
      })
    })
  })
}

function qrcode (url) {
  return new Promise((resolve) => qrcodeTerminal.generate(url, resolve))
}
