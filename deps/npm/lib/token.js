'use strict'

const profile = require('libnpm/profile')
const npm = require('./npm.js')
const figgyPudding = require('figgy-pudding')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const Table = require('cli-table3')
const Bluebird = require('bluebird')
const isCidrV4 = require('is-cidr').v4
const isCidrV6 = require('is-cidr').v6
const readUserInfo = require('./utils/read-user-info.js')
const ansistyles = require('ansistyles')
const log = require('npmlog')
const pulseTillDone = require('./utils/pulse-till-done.js')

module.exports = token

token._validateCIDRList = validateCIDRList

token.usage =
  'npm token list\n' +
  'npm token revoke <tokenKey>\n' +
  'npm token create [--read-only] [--cidr=list]\n'

token.subcommands = ['list', 'revoke', 'create']

token.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain

  switch (argv[2]) {
    case 'list':
    case 'revoke':
    case 'create':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

function withCb (prom, cb) {
  prom.then((value) => cb(null, value), cb)
}

function token (args, cb) {
  log.gauge.show('token')
  if (args.length === 0) return withCb(list([]), cb)
  switch (args[0]) {
    case 'list':
    case 'ls':
      withCb(list(), cb)
      break
    case 'delete':
    case 'revoke':
    case 'remove':
    case 'rm':
      withCb(rm(args.slice(1)), cb)
      break
    case 'create':
      withCb(create(args.slice(1)), cb)
      break
    default:
      cb(new Error('Unknown profile command: ' + args[0]))
  }
}

function generateTokenIds (tokens, minLength) {
  const byId = {}
  tokens.forEach((token) => {
    token.id = token.key
    for (let ii = minLength; ii < token.key.length; ++ii) {
      if (!tokens.some((ot) => ot !== token && ot.key.slice(0, ii) === token.key.slice(0, ii))) {
        token.id = token.key.slice(0, ii)
        break
      }
    }
    byId[token.id] = token
  })
  return byId
}

const TokenConfig = figgyPudding({
  registry: {},
  otp: {},
  cidr: {},
  'read-only': {},
  json: {},
  parseable: {}
})

function config () {
  let conf = TokenConfig(npmConfig())
  const creds = npm.config.getCredentialsByURI(conf.registry)
  if (creds.token) {
    conf = conf.concat({
      auth: { token: creds.token }
    })
  } else if (creds.username) {
    conf = conf.concat({
      auth: {
        basic: {
          username: creds.username,
          password: creds.password
        }
      }
    })
  } else if (creds.auth) {
    const auth = Buffer.from(creds.auth, 'base64').toString().split(':', 2)
    conf = conf.concat({
      auth: {
        basic: {
          username: auth[0],
          password: auth[1]
        }
      }
    })
  } else {
    conf = conf.concat({ auth: {} })
    conf.auth = {}
  }
  if (conf.otp) conf.auth.otp = conf.otp
  return conf
}

function list (args) {
  const conf = config()
  log.info('token', 'getting list')
  return pulseTillDone.withPromise(profile.listTokens(conf)).then((tokens) => {
    if (conf.json) {
      output(JSON.stringify(tokens, null, 2))
      return
    } else if (conf.parseable) {
      output(['key', 'token', 'created', 'readonly', 'CIDR whitelist'].join('\t'))
      tokens.forEach((token) => {
        output([
          token.key,
          token.token,
          token.created,
          token.readonly ? 'true' : 'false',
          token.cidr_whitelist ? token.cidr_whitelist.join(',') : ''
        ].join('\t'))
      })
      return
    }
    generateTokenIds(tokens, 6)
    const idWidth = tokens.reduce((acc, token) => Math.max(acc, token.id.length), 0)
    const table = new Table({
      head: ['id', 'token', 'created', 'readonly', 'CIDR whitelist'],
      colWidths: [Math.max(idWidth, 2) + 2, 9, 12, 10]
    })
    tokens.forEach((token) => {
      table.push([
        token.id,
        token.token + '…',
        String(token.created).slice(0, 10),
        token.readonly ? 'yes' : 'no',
        token.cidr_whitelist ? token.cidr_whitelist.join(', ') : ''
      ])
    })
    output(table.toString())
  })
}

function rm (args) {
  if (args.length === 0) {
    throw new Error('npm token revoke <tokenKey>')
  }
  const conf = config()
  const toRemove = []
  const progress = log.newItem('removing tokens', toRemove.length)
  progress.info('token', 'getting existing list')
  return pulseTillDone.withPromise(profile.listTokens(conf).then((tokens) => {
    args.forEach((id) => {
      const matches = tokens.filter((token) => token.key.indexOf(id) === 0)
      if (matches.length === 1) {
        toRemove.push(matches[0].key)
      } else if (matches.length > 1) {
        throw new Error(`Token ID "${id}" was ambiguous, a new token may have been created since you last ran \`npm-profile token list\`.`)
      } else {
        const tokenMatches = tokens.filter((token) => id.indexOf(token.token) === 0)
        if (tokenMatches === 0) {
          throw new Error(`Unknown token id or value "${id}".`)
        }
        toRemove.push(id)
      }
    })
    return Bluebird.map(toRemove, (key) => {
      return profile.removeToken(key, conf).catch((ex) => {
        if (ex.code !== 'EOTP') throw ex
        log.info('token', 'failed because revoking this token requires OTP')
        return readUserInfo.otp().then((otp) => {
          conf.auth.otp = otp
          return profile.removeToken(key, conf)
        })
      })
    })
  })).then(() => {
    if (conf.json) {
      output(JSON.stringify(toRemove))
    } else if (conf.parseable) {
      output(toRemove.join('\t'))
    } else {
      output('Removed ' + toRemove.length + ' token' + (toRemove.length !== 1 ? 's' : ''))
    }
  })
}

function create (args) {
  const conf = config()
  const cidr = conf.cidr
  const readonly = conf['read-only']

  const validCIDR = validateCIDRList(cidr)
  return readUserInfo.password().then((password) => {
    log.info('token', 'creating')
    return profile.createToken(password, readonly, validCIDR, conf).catch((ex) => {
      if (ex.code !== 'EOTP') throw ex
      log.info('token', 'failed because it requires OTP')
      return readUserInfo.otp().then((otp) => {
        conf.auth.otp = otp
        log.info('token', 'creating with OTP')
        return pulseTillDone.withPromise(profile.createToken(password, readonly, validCIDR, conf))
      })
    })
  }).then((result) => {
    delete result.key
    delete result.updated
    if (conf.json) {
      output(JSON.stringify(result))
    } else if (conf.parseable) {
      Object.keys(result).forEach((k) => output(k + '\t' + result[k]))
    } else {
      const table = new Table()
      Object.keys(result).forEach((k) => table.push({[ansistyles.bright(k)]: String(result[k])}))
      output(table.toString())
    }
  })
}

function validateCIDR (cidr) {
  if (isCidrV6(cidr)) {
    throw new Error('CIDR whitelist can only contain IPv4 addresses, ' + cidr + ' is IPv6')
  }
  if (!isCidrV4(cidr)) {
    throw new Error('CIDR whitelist contains invalid CIDR entry: ' + cidr)
  }
}

function validateCIDRList (cidrs) {
  const maybeList = cidrs ? (Array.isArray(cidrs) ? cidrs : [cidrs]) : []
  const list = maybeList.length === 1 ? maybeList[0].split(/,\s*/) : maybeList
  list.forEach(validateCIDR)
  return list
}
