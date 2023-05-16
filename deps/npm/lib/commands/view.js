const columns = require('cli-columns')
const fs = require('fs')
const jsonParse = require('json-parse-even-better-errors')
const log = require('../utils/log-shim.js')
const npa = require('npm-package-arg')
const { resolve } = require('path')
const formatBytes = require('../utils/format-bytes.js')
const relativeDate = require('tiny-relative-date')
const semver = require('semver')
const { inspect, promisify } = require('util')
const { packument } = require('pacote')

const readFile = promisify(fs.readFile)
const readJson = async file => jsonParse(await readFile(file, 'utf8'))

const Queryable = require('../utils/queryable.js')
const BaseCommand = require('../base-command.js')
class View extends BaseCommand {
  static description = 'View registry info'
  static name = 'view'
  static params = [
    'json',
    'workspace',
    'workspaces',
    'include-workspace-root',
  ]

  static workspaces = true
  static ignoreImplicitWorkspace = false
  static usage = ['[<package-spec>] [<field>[.subfield]...]']

  async completion (opts) {
    if (opts.conf.argv.remain.length <= 2) {
      // There used to be registry completion here, but it stopped
      // making sense somewhere around 50,000 packages on the registry
      return
    }
    // have the package, get the fields
    const config = {
      ...this.npm.flatOptions,
      fullMetadata: true,
      preferOnline: true,
    }
    const spec = npa(opts.conf.argv.remain[2])
    const pckmnt = await packument(spec, config)
    const defaultTag = this.npm.config.get('tag')
    const dv = pckmnt.versions[pckmnt['dist-tags'][defaultTag]]
    pckmnt.versions = Object.keys(pckmnt.versions).sort(semver.compareLoose)

    return getFields(pckmnt).concat(getFields(dv))

    function getFields (d, f, pref) {
      f = f || []
      pref = pref || []
      Object.keys(d).forEach((k) => {
        if (k.charAt(0) === '_' || k.indexOf('.') !== -1) {
          return
        }
        const p = pref.concat(k).join('.')
        f.push(p)
        if (Array.isArray(d[k])) {
          d[k].forEach((val, i) => {
            const pi = p + '[' + i + ']'
            if (val && typeof val === 'object') {
              getFields(val, f, [p])
            } else {
              f.push(pi)
            }
          })
          return
        }
        if (typeof d[k] === 'object') {
          getFields(d[k], f, [p])
        }
      })
      return f
    }
  }

  async exec (args) {
    if (!args.length) {
      args = ['.']
    }
    let pkg = args.shift()
    const local = /^\.@/.test(pkg) || pkg === '.'

    if (local) {
      if (this.npm.global) {
        throw new Error('Cannot use view command in global mode.')
      }
      const dir = this.npm.prefix
      const manifest = await readJson(resolve(dir, 'package.json'))
      if (!manifest.name) {
        throw new Error('Invalid package.json, no "name" field')
      }
      // put the version back if it existed
      pkg = `${manifest.name}${pkg.slice(1)}`
    }

    let wholePackument = false
    if (!args.length) {
      args = ['']
      wholePackument = true
    }
    const [pckmnt, data] = await this.getData(pkg, args)

    if (!this.npm.config.get('json') && wholePackument) {
      // pretty view (entire packument)
      data.map((v) => this.prettyView(pckmnt, v[Object.keys(v)[0]]['']))
    } else {
      // JSON formatted output (JSON or specific attributes from packument)
      let reducedData = data.reduce(reducer, {})
      if (wholePackument) {
        // No attributes
        reducedData = cleanBlanks(reducedData)
        log.silly('view', reducedData)
      }
      // disable the progress bar entirely, as we can't meaningfully update it
      // if we may have partial lines printed.
      log.disableProgress()

      const msg = await this.jsonData(reducedData, pckmnt._id)
      if (msg !== '') {
        this.npm.output(msg)
      }
    }
  }

  async execWorkspaces (args) {
    if (!args.length) {
      args = ['.']
    }

    const pkg = args.shift()

    const local = /^\.@/.test(pkg) || pkg === '.'
    if (!local) {
      log.warn('Ignoring workspaces for specified package(s)')
      return this.exec([pkg, ...args])
    }
    let wholePackument = false
    if (!args.length) {
      wholePackument = true
      args = [''] // getData relies on this
    }
    const results = {}
    await this.setWorkspaces()
    for (const name of this.workspaceNames) {
      const wsPkg = `${name}${pkg.slice(1)}`
      const [pckmnt, data] = await this.getData(wsPkg, args)

      let reducedData = data.reduce(reducer, {})
      if (wholePackument) {
        // No attributes
        reducedData = cleanBlanks(reducedData)
        log.silly('view', reducedData)
      }

      if (!this.npm.config.get('json')) {
        if (wholePackument) {
          data.map((v) => this.prettyView(pckmnt, v[Object.keys(v)[0]]['']))
        } else {
          this.npm.output(`${name}:`)
          const msg = await this.jsonData(reducedData, pckmnt._id)
          if (msg !== '') {
            this.npm.output(msg)
          }
        }
      } else {
        const msg = await this.jsonData(reducedData, pckmnt._id)
        if (msg !== '') {
          results[name] = JSON.parse(msg)
        }
      }
    }
    if (Object.keys(results).length > 0) {
      this.npm.output(JSON.stringify(results, null, 2))
    }
  }

  async getData (pkg, args) {
    const opts = {
      ...this.npm.flatOptions,
      preferOnline: true,
      fullMetadata: true,
    }

    const spec = npa(pkg)

    // get the data about this package
    let version = this.npm.config.get('tag')
    // rawSpec is the git url if this is from git
    if (spec.type !== 'git' && spec.type !== 'directory' && spec.rawSpec !== '*') {
      version = spec.rawSpec
    }

    const pckmnt = await packument(spec, opts)

    if (pckmnt['dist-tags']?.[version]) {
      version = pckmnt['dist-tags'][version]
    }

    if (pckmnt.time && pckmnt.time.unpublished) {
      const u = pckmnt.time.unpublished
      const er = new Error(`Unpublished on ${u.time}`)
      er.statusCode = 404
      er.code = 'E404'
      er.pkgid = pckmnt._id
      throw er
    }

    const data = []
    const versions = pckmnt.versions || {}
    pckmnt.versions = Object.keys(versions).sort(semver.compareLoose)

    // remove readme unless we asked for it
    if (args.indexOf('readme') === -1) {
      delete pckmnt.readme
    }

    Object.keys(versions).forEach((v) => {
      if (semver.satisfies(v, version, true)) {
        args.forEach(arg => {
          // remove readme unless we asked for it
          if (args.indexOf('readme') !== -1) {
            delete versions[v].readme
          }

          data.push(showFields(pckmnt, versions[v], arg))
        })
      }
    })

    // No data has been pushed because no data is matching the specified version
    if (data.length === 0 && version !== 'latest') {
      const er = new Error(`No match found for version ${version}`)
      er.statusCode = 404
      er.code = 'E404'
      er.pkgid = `${pckmnt._id}@${version}`
      throw er
    }

    if (
      !this.npm.config.get('json') &&
      args.length === 1 &&
      args[0] === ''
    ) {
      pckmnt.version = version
    }

    return [pckmnt, data]
  }

  async jsonData (data, name) {
    const versions = Object.keys(data)
    let msg = ''
    let msgJson = []
    const includeVersions = versions.length > 1
    let includeFields
    const json = this.npm.config.get('json')

    versions.forEach((v) => {
      const fields = Object.keys(data[v])
      includeFields = includeFields || (fields.length > 1)
      if (json) {
        msgJson.push({})
      }
      fields.forEach((f) => {
        let d = cleanup(data[v][f])
        if (fields.length === 1 && json) {
          msgJson[msgJson.length - 1][f] = d
        }

        if (includeVersions || includeFields || typeof d !== 'string') {
          if (json) {
            msgJson[msgJson.length - 1][f] = d
          } else {
            d = inspect(d, {
              showHidden: false,
              depth: 5,
              colors: this.npm.color,
              maxArrayLength: null,
            })
          }
        } else if (typeof d === 'string' && json) {
          d = JSON.stringify(d)
        }

        if (!json) {
          if (f && includeFields) {
            f += ' = '
          }
          msg += (includeVersions ? name + '@' + v + ' ' : '') +
            (includeFields ? f : '') + d + '\n'
        }
      })
    })

    if (json) {
      if (msgJson.length && Object.keys(msgJson[0]).length === 1) {
        const k = Object.keys(msgJson[0])[0]
        msgJson = msgJson.map(m => m[k])
      }
      if (msgJson.length === 1) {
        msg = JSON.stringify(msgJson[0], null, 2) + '\n'
      } else if (msgJson.length > 1) {
        msg = JSON.stringify(msgJson, null, 2) + '\n'
      }
    }

    return msg.trim()
  }

  prettyView (packu, manifest) {
    // More modern, pretty printing of default view
    const unicode = this.npm.config.get('unicode')
    const chalk = this.npm.chalk
    const tags = []

    Object.keys(packu['dist-tags']).forEach((t) => {
      const version = packu['dist-tags'][t]
      tags.push(`${chalk.bold.green(t)}: ${version}`)
    })
    const unpackedSize = manifest.dist.unpackedSize &&
      formatBytes(manifest.dist.unpackedSize, true)
    const licenseField = manifest.license || 'Proprietary'
    const info = {
      name: chalk.green(manifest.name),
      version: chalk.green(manifest.version),
      bins: Object.keys(manifest.bin || {}),
      versions: chalk.yellow(packu.versions.length + ''),
      description: manifest.description,
      deprecated: manifest.deprecated,
      keywords: packu.keywords || [],
      license: typeof licenseField === 'string'
        ? licenseField
        : (licenseField.type || 'Proprietary'),
      deps: Object.keys(manifest.dependencies || {}).map((dep) => {
        return `${chalk.yellow(dep)}: ${manifest.dependencies[dep]}`
      }),
      publisher: manifest._npmUser && unparsePerson({
        name: chalk.yellow(manifest._npmUser.name),
        email: chalk.cyan(manifest._npmUser.email),
      }),
      modified: !packu.time ? undefined
      : chalk.yellow(relativeDate(packu.time[manifest.version])),
      maintainers: (packu.maintainers || []).map((u) => unparsePerson({
        name: chalk.yellow(u.name),
        email: chalk.cyan(u.email),
      })),
      repo: (
        manifest.bugs && (manifest.bugs.url || manifest.bugs)
      ) || (
        manifest.repository && (manifest.repository.url || manifest.repository)
      ),
      site: (
        manifest.homepage && (manifest.homepage.url || manifest.homepage)
      ),
      tags,
      tarball: chalk.cyan(manifest.dist.tarball),
      shasum: chalk.yellow(manifest.dist.shasum),
      integrity:
        manifest.dist.integrity && chalk.yellow(manifest.dist.integrity),
      fileCount:
        manifest.dist.fileCount && chalk.yellow(manifest.dist.fileCount),
      unpackedSize: unpackedSize && chalk.yellow(unpackedSize),
    }
    if (info.license.toLowerCase().trim() === 'proprietary') {
      info.license = chalk.bold.red(info.license)
    } else {
      info.license = chalk.green(info.license)
    }

    this.npm.output('')
    this.npm.output(
      chalk.underline.bold(`${info.name}@${info.version}`) +
      ' | ' + info.license +
      ' | deps: ' + (info.deps.length ? chalk.cyan(info.deps.length) : chalk.green('none')) +
      ' | versions: ' + info.versions
    )
    info.description && this.npm.output(info.description)
    if (info.repo || info.site) {
      info.site && this.npm.output(chalk.cyan(info.site))
    }

    const warningSign = unicode ? ' ⚠️ ' : '!!'
    info.deprecated && this.npm.output(
      `\n${chalk.bold.red('DEPRECATED')}${
      warningSign
    } - ${info.deprecated}`
    )

    if (info.keywords.length) {
      this.npm.output('')
      this.npm.output('keywords:', chalk.yellow(info.keywords.join(', ')))
    }

    if (info.bins.length) {
      this.npm.output('')
      this.npm.output('bin:', chalk.yellow(info.bins.join(', ')))
    }

    this.npm.output('')
    this.npm.output('dist')
    this.npm.output('.tarball:', info.tarball)
    this.npm.output('.shasum:', info.shasum)
    info.integrity && this.npm.output('.integrity:', info.integrity)
    info.unpackedSize && this.npm.output('.unpackedSize:', info.unpackedSize)

    const maxDeps = 24
    if (info.deps.length) {
      this.npm.output('')
      this.npm.output('dependencies:')
      this.npm.output(columns(info.deps.slice(0, maxDeps), { padding: 1 }))
      if (info.deps.length > maxDeps) {
        this.npm.output(`(...and ${info.deps.length - maxDeps} more.)`)
      }
    }

    if (info.maintainers && info.maintainers.length) {
      this.npm.output('')
      this.npm.output('maintainers:')
      info.maintainers.forEach((u) => this.npm.output('-', u))
    }

    this.npm.output('')
    this.npm.output('dist-tags:')
    this.npm.output(columns(info.tags))

    if (info.publisher || info.modified) {
      let publishInfo = 'published'
      if (info.modified) {
        publishInfo += ` ${info.modified}`
      }
      if (info.publisher) {
        publishInfo += ` by ${info.publisher}`
      }
      this.npm.output('')
      this.npm.output(publishInfo)
    }
  }
}
module.exports = View

function cleanBlanks (obj) {
  const clean = {}
  Object.keys(obj).forEach((version) => {
    clean[version] = obj[version]['']
  })
  return clean
}

// takes an array of objects and merges them into one object
function reducer (acc, cur) {
  if (cur) {
    Object.keys(cur).forEach((v) => {
      acc[v] = acc[v] || {}
      Object.keys(cur[v]).forEach((t) => {
        acc[v][t] = cur[v][t]
      })
    })
  }

  return acc
}

// return whatever was printed
function showFields (data, version, fields) {
  const o = {}
  ;[data, version].forEach((s) => {
    Object.keys(s).forEach((k) => {
      o[k] = s[k]
    })
  })

  const queryable = new Queryable(o)
  const s = queryable.query(fields)
  const res = { [version.version]: s }

  if (s) {
    return res
  }
}

function cleanup (data) {
  if (Array.isArray(data)) {
    return data.map(cleanup)
  }

  if (!data || typeof data !== 'object') {
    return data
  }

  const keys = Object.keys(data)
  if (keys.length <= 3 &&
      data.name &&
      (keys.length === 1 ||
       (keys.length === 3 && data.email && data.url) ||
       (keys.length === 2 && (data.email || data.url)))) {
    data = unparsePerson(data)
  }

  return data
}

function unparsePerson (d) {
  return d.name +
    (d.email ? ' <' + d.email + '>' : '') +
    (d.url ? ' (' + d.url + ')' : '')
}
