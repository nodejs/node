// npm view [pkg [pkg ...]]

const color = require('ansicolors')
const columns = require('cli-columns')
const fs = require('fs')
const jsonParse = require('json-parse-even-better-errors')
const log = require('../utils/log-shim.js')
const npa = require('npm-package-arg')
const { resolve } = require('path')
const formatBytes = require('../utils/format-bytes.js')
const relativeDate = require('tiny-relative-date')
const semver = require('semver')
const style = require('ansistyles')
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

  static usage = ['[<@scope>/]<pkg>[@<version>] [<field>[.subfield]...]']

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
      if (!d) {
        return f
      }
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
      if (this.npm.config.get('global')) {
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
        console.log(msg)
      }
    }
  }

  async execWorkspaces (args, filters) {
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
    await this.setWorkspaces(filters)
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
          console.log(`${name}:`)
          const msg = await this.jsonData(reducedData, pckmnt._id)
          if (msg !== '') {
            console.log(msg)
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
      console.log(JSON.stringify(results, null, 2))
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
    if (spec.type !== 'git' && spec.rawSpec) {
      version = spec.rawSpec
    }

    const pckmnt = await packument(spec, opts)

    if (pckmnt['dist-tags'] && pckmnt['dist-tags'][version]) {
      version = pckmnt['dist-tags'][version]
    }

    if (pckmnt.time && pckmnt.time.unpublished) {
      const u = pckmnt.time.unpublished
      const er = new Error('Unpublished by ' + u.name + ' on ' + u.time)
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

  prettyView (packument, manifest) {
    // More modern, pretty printing of default view
    const unicode = this.npm.config.get('unicode')
    const tags = []

    Object.keys(packument['dist-tags']).forEach((t) => {
      const version = packument['dist-tags'][t]
      tags.push(`${style.bright(color.green(t))}: ${version}`)
    })
    const unpackedSize = manifest.dist.unpackedSize &&
      formatBytes(manifest.dist.unpackedSize, true)
    const licenseField = manifest.license || 'Proprietary'
    const info = {
      name: color.green(manifest.name),
      version: color.green(manifest.version),
      bins: Object.keys(manifest.bin || {}).map(color.yellow),
      versions: color.yellow(packument.versions.length + ''),
      description: manifest.description,
      deprecated: manifest.deprecated,
      keywords: (packument.keywords || []).map(color.yellow),
      license: typeof licenseField === 'string'
        ? licenseField
        : (licenseField.type || 'Proprietary'),
      deps: Object.keys(manifest.dependencies || {}).map((dep) => {
        return `${color.yellow(dep)}: ${manifest.dependencies[dep]}`
      }),
      publisher: manifest._npmUser && unparsePerson({
        name: color.yellow(manifest._npmUser.name),
        email: color.cyan(manifest._npmUser.email),
      }),
      modified: !packument.time ? undefined
      : color.yellow(relativeDate(packument.time[manifest.version])),
      maintainers: (packument.maintainers || []).map((u) => unparsePerson({
        name: color.yellow(u.name),
        email: color.cyan(u.email),
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
      tarball: color.cyan(manifest.dist.tarball),
      shasum: color.yellow(manifest.dist.shasum),
      integrity:
        manifest.dist.integrity && color.yellow(manifest.dist.integrity),
      fileCount:
        manifest.dist.fileCount && color.yellow(manifest.dist.fileCount),
      unpackedSize: unpackedSize && color.yellow(unpackedSize),
    }
    if (info.license.toLowerCase().trim() === 'proprietary') {
      info.license = style.bright(color.red(info.license))
    } else {
      info.license = color.green(info.license)
    }

    console.log('')
    console.log(
      style.underline(style.bright(`${info.name}@${info.version}`)) +
      ' | ' + info.license +
      ' | deps: ' + (info.deps.length ? color.cyan(info.deps.length) : color.green('none')) +
      ' | versions: ' + info.versions
    )
    info.description && console.log(info.description)
    if (info.repo || info.site) {
      info.site && console.log(color.cyan(info.site))
    }

    const warningSign = unicode ? ' ⚠️ ' : '!!'
    info.deprecated && console.log(
      `\n${style.bright(color.red('DEPRECATED'))}${
      warningSign
    } - ${info.deprecated}`
    )

    if (info.keywords.length) {
      console.log('')
      console.log('keywords:', info.keywords.join(', '))
    }

    if (info.bins.length) {
      console.log('')
      console.log('bin:', info.bins.join(', '))
    }

    console.log('')
    console.log('dist')
    console.log('.tarball:', info.tarball)
    console.log('.shasum:', info.shasum)
    info.integrity && console.log('.integrity:', info.integrity)
    info.unpackedSize && console.log('.unpackedSize:', info.unpackedSize)

    const maxDeps = 24
    if (info.deps.length) {
      console.log('')
      console.log('dependencies:')
      console.log(columns(info.deps.slice(0, maxDeps), { padding: 1 }))
      if (info.deps.length > maxDeps) {
        console.log(`(...and ${info.deps.length - maxDeps} more.)`)
      }
    }

    if (info.maintainers && info.maintainers.length) {
      console.log('')
      console.log('maintainers:')
      info.maintainers.forEach((u) => console.log('-', u))
    }

    console.log('')
    console.log('dist-tags:')
    console.log(columns(info.tags))

    if (info.publisher || info.modified) {
      let publishInfo = 'published'
      if (info.modified) {
        publishInfo += ` ${info.modified}`
      }
      if (info.publisher) {
        publishInfo += ` by ${info.publisher}`
      }
      console.log('')
      console.log(publishInfo)
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
