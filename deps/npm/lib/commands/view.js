const columns = require('cli-columns')
const { readFile } = require('fs/promises')
const jsonParse = require('json-parse-even-better-errors')
const { log, output } = require('proc-log')
const npa = require('npm-package-arg')
const { resolve } = require('path')
const formatBytes = require('../utils/format-bytes.js')
const relativeDate = require('tiny-relative-date')
const semver = require('semver')
const { inspect } = require('util')
const { packument } = require('pacote')
const Queryable = require('../utils/queryable.js')
const BaseCommand = require('../base-cmd.js')

const readJson = async file => jsonParse(await readFile(file, 'utf8'))

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

  static async completion (opts, npm) {
    if (opts.conf.argv.remain.length <= 2) {
      // There used to be registry completion here, but it stopped
      // making sense somewhere around 50,000 packages on the registry
      return
    }
    // have the package, get the fields
    const config = {
      ...npm.flatOptions,
      fullMetadata: true,
      preferOnline: true,
    }
    const spec = npa(opts.conf.argv.remain[2])
    const pckmnt = await packument(spec, config)
    const defaultTag = npm.config.get('tag')
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

      const msg = await this.jsonData(reducedData, pckmnt._id)
      if (msg !== '') {
        output.standard(msg)
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
          output.standard(`${name}:`)
          const msg = await this.jsonData(reducedData, pckmnt._id)
          if (msg !== '') {
            output.standard(msg)
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
      output.standard(JSON.stringify(results, null, 2))
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
    pckmnt.versions = Object.keys(versions).filter(v => {
      if (semver.valid(v)) {
        return true
      }
      log.info('view', `Ignoring invalid version: ${v}`)
      return false
    }).sort(semver.compareLoose)

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
    const deps = Object.keys(manifest.dependencies || {}).map((dep) =>
      `${chalk.blue(dep)}: ${manifest.dependencies[dep]}`
    )
    const site = manifest.homepage?.url || manifest.homepage
    const bins = Object.keys(manifest.bin || {})
    const licenseField = manifest.license || 'Proprietary'
    const license = typeof licenseField === 'string'
      ? licenseField
      : (licenseField.type || 'Proprietary')

    output.standard('')
    output.standard([
      chalk.underline.cyan(`${manifest.name}@${manifest.version}`),
      license.toLowerCase().trim() === 'proprietary'
        ? chalk.red(license)
        : chalk.green(license),
      `deps: ${deps.length ? chalk.cyan(deps.length) : chalk.cyan('none')}`,
      `versions: ${chalk.cyan(packu.versions.length + '')}`,
    ].join(' | '))

    manifest.description && output.standard(manifest.description)
    if (site) {
      output.standard(chalk.blue(site))
    }

    manifest.deprecated && output.standard(
      `\n${chalk.redBright('DEPRECATED')}${unicode ? ' ⚠️ ' : '!!'} - ${manifest.deprecated}`
    )

    if (packu.keywords?.length) {
      output.standard(`\nkeywords: ${
        packu.keywords.map(k => chalk.cyan(k)).join(', ')
      }`)
    }

    if (bins.length) {
      output.standard(`\nbin: ${chalk.cyan(bins.join(', '))}`)
    }

    output.standard('\ndist')
    output.standard(`.tarball: ${chalk.blue(manifest.dist.tarball)}`)
    output.standard(`.shasum: ${chalk.green(manifest.dist.shasum)}`)
    if (manifest.dist.integrity) {
      output.standard(`.integrity: ${chalk.green(manifest.dist.integrity)}`)
    }
    if (manifest.dist.unpackedSize) {
      output.standard(`.unpackedSize: ${chalk.blue(formatBytes(manifest.dist.unpackedSize, true))}`)
    }

    if (deps.length) {
      const maxDeps = 24
      output.standard('\ndependencies:')
      output.standard(columns(deps.slice(0, maxDeps), { padding: 1 }))
      if (deps.length > maxDeps) {
        output.standard(chalk.dim(`(...and ${deps.length - maxDeps} more.)`))
      }
    }

    if (packu.maintainers?.length) {
      output.standard('\nmaintainers:')
      packu.maintainers.forEach(u =>
        output.standard(`- ${unparsePerson({
          name: chalk.blue(u.name),
          email: chalk.dim(u.email) })}`)
      )
    }

    output.standard('\ndist-tags:')
    output.standard(columns(Object.keys(packu['dist-tags']).map(t =>
      `${chalk.blue(t)}: ${packu['dist-tags'][t]}`
    )))

    const publisher = manifest._npmUser && unparsePerson({
      name: chalk.blue(manifest._npmUser.name),
      email: chalk.dim(manifest._npmUser.email),
    })
    if (publisher || packu.time) {
      let publishInfo = 'published'
      if (packu.time) {
        publishInfo += ` ${chalk.cyan(relativeDate(packu.time[manifest.version]))}`
      }
      if (publisher) {
        publishInfo += ` by ${publisher}`
      }
      output.standard('')
      output.standard(publishInfo)
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
