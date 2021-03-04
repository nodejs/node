// npm view [pkg [pkg ...]]

const byteSize = require('byte-size')
const color = require('ansicolors')
const columns = require('cli-columns')
const fs = require('fs')
const jsonParse = require('json-parse-even-better-errors')
const log = require('npmlog')
const npa = require('npm-package-arg')
const path = require('path')
const relativeDate = require('tiny-relative-date')
const semver = require('semver')
const style = require('ansistyles')
const { inspect, promisify } = require('util')
const { packument } = require('pacote')

const usageUtil = require('./utils/usage.js')

const readFile = promisify(fs.readFile)
const readJson = async file => jsonParse(await readFile(file, 'utf8'))

class View {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil(
      'view',
      'npm view [<@scope>/]<pkg>[@<version>] [<field>[.subfield]...]'
    )
  }

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
    const { defaultTag } = config
    const spec = npa(opts.conf.argv.remain[2])
    const pckmnt = await packument(spec, config)
    const dv = pckmnt.versions[pckmnt['dist-tags'][defaultTag]]
    pckmnt.versions = Object.keys(pckmnt.versions).sort(semver.compareLoose)

    return getFields(pckmnt).concat(getFields(dv))

    function getFields (d, f, pref) {
      f = f || []
      if (!d)
        return f
      pref = pref || []
      Object.keys(d).forEach((k) => {
        if (k.charAt(0) === '_' || k.indexOf('.') !== -1)
          return
        const p = pref.concat(k).join('.')
        f.push(p)
        if (Array.isArray(d[k])) {
          d[k].forEach((val, i) => {
            const pi = p + '[' + i + ']'
            if (val && typeof val === 'object')
              getFields(val, f, [p])
            else
              f.push(pi)
          })
          return
        }
        if (typeof d[k] === 'object')
          getFields(d[k], f, [p])
      })
      return f
    }
  }

  exec (args, cb) {
    this.view(args).then(() => cb()).catch(cb)
  }

  async view (args) {
    if (!args.length)
      args = ['.']

    const opts = {
      ...this.npm.flatOptions,
      preferOnline: true,
      fullMetadata: true,
    }
    const pkg = args.shift()
    let nv
    if (/^[.]@/.test(pkg))
      nv = npa.resolve(null, pkg.slice(2))
    else
      nv = npa(pkg)

    const name = nv.name
    const local = (name === '.' || !name)

    if (opts.global && local)
      throw new Error('Cannot use view command in global mode.')

    if (local) {
      const dir = this.npm.prefix
      const manifest = await readJson(path.resolve(dir, 'package.json'))
      if (!manifest.name)
        throw new Error('Invalid package.json, no "name" field')
      const p = manifest.name
      nv = npa(p)
      if (pkg && ~pkg.indexOf('@'))
        nv.rawSpec = pkg.split('@')[pkg.indexOf('@')]
    }

    // get the data about this package
    let version = nv.rawSpec || this.npm.flatOptions.defaultTag

    const pckmnt = await packument(nv, opts)

    if (pckmnt['dist-tags'] && pckmnt['dist-tags'][version])
      version = pckmnt['dist-tags'][version]

    if (pckmnt.time && pckmnt.time.unpublished) {
      const u = pckmnt.time.unpublished
      const er = new Error('Unpublished by ' + u.name + ' on ' + u.time)
      er.statusCode = 404
      er.code = 'E404'
      er.pkgid = pckmnt._id
      throw er
    }

    const results = []
    const versions = pckmnt.versions || {}
    pckmnt.versions = Object.keys(versions).sort(semver.compareLoose)
    if (!args.length)
      args = ['']

    // remove readme unless we asked for it
    if (args.indexOf('readme') === -1)
      delete pckmnt.readme

    Object.keys(versions).forEach((v) => {
      if (semver.satisfies(v, version, true)) {
        args.forEach(arg => {
          // remove readme unless we asked for it
          if (args.indexOf('readme') !== -1)
            delete versions[v].readme

          results.push(showFields(pckmnt, versions[v], arg))
        })
      }
    })
    let retval = results.reduce(reducer, {})

    if (args.length === 1 && args[0] === '') {
      retval = cleanBlanks(retval)
      log.silly('view', retval)
    }

    if (
      !opts.json &&
      args.length === 1 &&
      args[0] === ''
    ) {
      // general view
      pckmnt.version = version
      await Promise.all(
        results.map((v) => this.prettyView(pckmnt, v[Object.keys(v)[0]][''], opts))
      )
      return retval
    } else {
      // view by field name
      await this.printData(retval, pckmnt._id, opts)
      return retval
    }
  }

  async printData (data, name, opts) {
    const versions = Object.keys(data)
    let msg = ''
    let msgJson = []
    const includeVersions = versions.length > 1
    let includeFields

    versions.forEach((v) => {
      const fields = Object.keys(data[v])
      includeFields = includeFields || (fields.length > 1)
      if (opts.json)
        msgJson.push({})
      fields.forEach((f) => {
        let d = cleanup(data[v][f])
        if (fields.length === 1 && opts.json)
          msgJson[msgJson.length - 1][f] = d

        if (includeVersions || includeFields || typeof d !== 'string') {
          if (opts.json)
            msgJson[msgJson.length - 1][f] = d
          else {
            d = inspect(d, {
              showHidden: false,
              depth: 5,
              colors: this.npm.color,
              maxArrayLength: null,
            })
          }
        } else if (typeof d === 'string' && opts.json)
          d = JSON.stringify(d)

        if (!opts.json) {
          if (f && includeFields)
            f += ' = '
          msg += (includeVersions ? name + '@' + v + ' ' : '') +
            (includeFields ? f : '') + d + '\n'
        }
      })
    })

    if (opts.json) {
      if (msgJson.length && Object.keys(msgJson[0]).length === 1) {
        const k = Object.keys(msgJson[0])[0]
        msgJson = msgJson.map(m => m[k])
      }
      if (msgJson.length === 1)
        msg = JSON.stringify(msgJson[0], null, 2) + '\n'
      else if (msgJson.length > 1)
        msg = JSON.stringify(msgJson, null, 2) + '\n'
    }

    // disable the progress bar entirely, as we can't meaningfully update it if
    // we may have partial lines printed.
    log.disableProgress()

    // only log if there is something to log
    if (msg !== '')
      console.log(msg.trim())
  }

  async prettyView (packument, manifest, opts) {
    // More modern, pretty printing of default view
    const unicode = opts.unicode
    const tags = []

    Object.keys(packument['dist-tags']).forEach((t) => {
      const version = packument['dist-tags'][t]
      tags.push(`${style.bright(color.green(t))}: ${version}`)
    })
    const unpackedSize = manifest.dist.unpackedSize &&
      byteSize(manifest.dist.unpackedSize)
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
      : color.yellow(relativeDate(packument.time[packument.version])),
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
      unpackedSize: unpackedSize && color.yellow(unpackedSize.value) + ' ' + unpackedSize.unit,
    }
    if (info.license.toLowerCase().trim() === 'proprietary')
      info.license = style.bright(color.red(info.license))
    else
      info.license = color.green(info.license)

    console.log('')
    console.log(
      style.underline(style.bright(`${info.name}@${info.version}`)) +
      ' | ' + info.license +
      ' | deps: ' + (info.deps.length ? color.cyan(info.deps.length) : color.green('none')) +
      ' | versions: ' + info.versions
    )
    info.description && console.log(info.description)
    if (info.repo || info.site)
      info.site && console.log(color.cyan(info.site))

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
      if (info.deps.length > maxDeps)
        console.log(`(...and ${info.deps.length - maxDeps} more.)`)
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
      if (info.modified)
        publishInfo += ` ${info.modified}`
      if (info.publisher)
        publishInfo += ` by ${info.publisher}`
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

function reducer (l, r) {
  if (r) {
    Object.keys(r).forEach((v) => {
      l[v] = l[v] || {}
      Object.keys(r[v]).forEach((t) => {
        l[v][t] = r[v][t]
      })
    })
  }

  return l
}

// return whatever was printed
function showFields (data, version, fields) {
  const o = {}
  ;[data, version].forEach((s) => {
    Object.keys(s).forEach((k) => {
      o[k] = s[k]
    })
  })
  return search(o, fields.split('.'), version.version, fields)
}

function search (data, fields, version, title) {
  let field
  const tail = fields
  while (!field && fields.length)
    field = tail.shift()
  fields = [field].concat(tail)
  let o
  if (!field && !tail.length) {
    o = {}
    o[version] = {}
    o[version][title] = data
    return o
  }
  let index = field.match(/(.+)\[([^\]]+)\]$/)
  if (index) {
    field = index[1]
    index = index[2]
    if (data[field] && data[field][index])
      return search(data[field][index], tail, version, title)
    else
      field = field + '[' + index + ']'
  }
  if (Array.isArray(data)) {
    if (data.length === 1)
      return search(data[0], fields, version, title)

    let results = []
    data.forEach((data, i) => {
      const tl = title.length
      const newt = title.substr(0, tl - fields.join('.').length - 1) +
                 '[' + i + ']' + [''].concat(fields).join('.')
      results.push(search(data, fields.slice(), version, newt))
    })
    results = results.reduce(reducer, {})
    return results
  }
  if (!data[field])
    return undefined
  data = data[field]
  if (tail.length) {
    // there are more fields to deal with.
    return search(data, tail, version, title)
  }
  o = {}
  o[version] = {}
  o[version][title] = data
  return o
}

function cleanup (data) {
  if (Array.isArray(data))
    return data.map(cleanup)

  if (!data || typeof data !== 'object')
    return data

  const keys = Object.keys(data)
  if (keys.length <= 3 &&
      data.name &&
      (keys.length === 1 ||
       (keys.length === 3 && data.email && data.url) ||
       (keys.length === 2 && (data.email || data.url))))
    data = unparsePerson(data)

  return data
}

function unparsePerson (d) {
  return d.name +
    (d.email ? ' <' + d.email + '>' : '') +
    (d.url ? ' (' + d.url + ')' : '')
}
