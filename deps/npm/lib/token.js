const Table = require('cli-table3')
const ansistyles = require('ansistyles')
const { v4: isCidrV4, v6: isCidrV6 } = require('is-cidr')
const log = require('npmlog')
const profile = require('npm-profile')

const otplease = require('./utils/otplease.js')
const pulseTillDone = require('./utils/pulse-till-done.js')
const readUserInfo = require('./utils/read-user-info.js')

const BaseCommand = require('./base-command.js')
class Token extends BaseCommand {
  static get description () {
    return 'Manage your authentication tokens'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'token'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      'list',
      'revoke <id|token>',
      'create [--read-only] [--cidr=list]',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'read-only',
      'cidr',
      'registry',
      'otp',
    ]
  }

  async completion (opts) {
    const argv = opts.conf.argv.remain
    const subcommands = ['list', 'revoke', 'create']
    if (argv.length === 2)
      return subcommands

    if (subcommands.includes(argv[2]))
      return []

    throw new Error(argv[2] + ' not recognized')
  }

  exec (args, cb) {
    this.token(args).then(() => cb()).catch(cb)
  }

  async token (args, cb) {
    log.gauge.show('token')
    if (args.length === 0)
      return this.list()
    switch (args[0]) {
      case 'list':
      case 'ls':
        return this.list()
      case 'delete':
      case 'revoke':
      case 'remove':
      case 'rm':
        return this.rm(args.slice(1))
      case 'create':
        return this.create(args.slice(1))
      default:
        throw this.usageError(`${args[0]} is not a recognized subcommand.`)
    }
  }

  async list () {
    const conf = this.config()
    log.info('token', 'getting list')
    const tokens = await pulseTillDone.withPromise(profile.listTokens(conf))
    if (conf.json) {
      this.npm.output(JSON.stringify(tokens, null, 2))
      return
    } else if (conf.parseable) {
      this.npm.output(['key', 'token', 'created', 'readonly', 'CIDR whitelist'].join('\t'))
      tokens.forEach((token) => {
        this.npm.output([
          token.key,
          token.token,
          token.created,
          token.readonly ? 'true' : 'false',
          token.cidr_whitelist ? token.cidr_whitelist.join(',') : '',
        ].join('\t'))
      })
      return
    }
    this.generateTokenIds(tokens, 6)
    const idWidth = tokens.reduce((acc, token) =>
      Math.max(acc, token.id.length), 0)
    const table = new Table({
      head: ['id', 'token', 'created', 'readonly', 'CIDR whitelist'],
      colWidths: [Math.max(idWidth, 2) + 2, 9, 12, 10],
    })
    tokens.forEach((token) => {
      table.push([
        token.id,
        token.token + '…',
        String(token.created).slice(0, 10),
        token.readonly ? 'yes' : 'no',
        token.cidr_whitelist ? token.cidr_whitelist.join(', ') : '',
      ])
    })
    this.npm.output(table.toString())
  }

  async rm (args) {
    if (args.length === 0)
      throw this.usageError('`<tokenKey>` argument is required.')

    const conf = this.config()
    const toRemove = []
    const progress = log.newItem('removing tokens', toRemove.length)
    progress.info('token', 'getting existing list')
    const tokens = await pulseTillDone.withPromise(profile.listTokens(conf))
    args.forEach((id) => {
      const matches = tokens.filter((token) => token.key.indexOf(id) === 0)
      if (matches.length === 1)
        toRemove.push(matches[0].key)
      else if (matches.length > 1)
        throw new Error(`Token ID "${id}" was ambiguous, a new token may have been created since you last ran \`npm token list\`.`)
      else {
        const tokenMatches = tokens.some(t => id.indexOf(t.token) === 0)
        if (!tokenMatches)
          throw new Error(`Unknown token id or value "${id}".`)

        toRemove.push(id)
      }
    })
    await Promise.all(toRemove.map(key => {
      return otplease(conf, conf => {
        return profile.removeToken(key, conf)
      })
    }))
    if (conf.json)
      this.npm.output(JSON.stringify(toRemove))
    else if (conf.parseable)
      this.npm.output(toRemove.join('\t'))
    else
      this.npm.output('Removed ' + toRemove.length + ' token' + (toRemove.length !== 1 ? 's' : ''))
  }

  async create (args) {
    const conf = this.config()
    const cidr = conf.cidr
    const readonly = conf.readOnly

    return readUserInfo.password().then((password) => {
      const validCIDR = this.validateCIDRList(cidr)
      log.info('token', 'creating')
      return pulseTillDone.withPromise(otplease(conf, conf => {
        return profile.createToken(password, readonly, validCIDR, conf)
      }))
    }).then((result) => {
      delete result.key
      delete result.updated
      if (conf.json)
        this.npm.output(JSON.stringify(result))
      else if (conf.parseable)
        Object.keys(result).forEach((k) => this.npm.output(k + '\t' + result[k]))
      else {
        const table = new Table()
        for (const k of Object.keys(result))
          table.push({ [ansistyles.bright(k)]: String(result[k]) })
        this.npm.output(table.toString())
      }
    })
  }

  config () {
    const conf = { ...this.npm.flatOptions }
    const creds = this.npm.config.getCredentialsByURI(conf.registry)
    if (creds.token)
      conf.auth = { token: creds.token }
    else if (creds.username) {
      conf.auth = {
        basic: {
          username: creds.username,
          password: creds.password,
        },
      }
    } else if (creds.auth) {
      const auth = Buffer.from(creds.auth, 'base64').toString().split(':', 2)
      conf.auth = {
        basic: {
          username: auth[0],
          password: auth[1],
        },
      }
    } else
      conf.auth = {}

    if (conf.otp)
      conf.auth.otp = conf.otp
    return conf
  }

  invalidCIDRError (msg) {
    return Object.assign(new Error(msg), { code: 'EINVALIDCIDR' })
  }

  generateTokenIds (tokens, minLength) {
    const byId = {}
    for (const token of tokens) {
      token.id = token.key
      for (let ii = minLength; ii < token.key.length; ++ii) {
        const match = tokens.some(ot =>
          ot !== token &&
          ot.key.slice(0, ii) === token.key.slice(0, ii))
        if (!match) {
          token.id = token.key.slice(0, ii)
          break
        }
      }
      byId[token.id] = token
    }
    return byId
  }

  validateCIDRList (cidrs) {
    const maybeList = cidrs ? (Array.isArray(cidrs) ? cidrs : [cidrs]) : []
    const list = maybeList.length === 1 ? maybeList[0].split(/,\s*/) : maybeList
    for (const cidr of list) {
      if (isCidrV6(cidr))
        throw this.invalidCIDRError('CIDR whitelist can only contain IPv4 addresses, ' + cidr + ' is IPv6')

      if (!isCidrV4(cidr))
        throw this.invalidCIDRError('CIDR whitelist contains invalid CIDR entry: ' + cidr)
    }
    return list
  }
}
module.exports = Token
