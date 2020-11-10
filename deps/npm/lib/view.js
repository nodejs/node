'use strict'

// npm view [pkg [pkg ...]]
module.exports = view

const BB = require('bluebird')

const byteSize = require('byte-size')
const color = require('ansicolors')
const columns = require('cli-columns')
const npmConfig = require('./config/figgy-config.js')
const log = require('npmlog')
const figgyPudding = require('figgy-pudding')
const npa = require('libnpm/parse-arg')
const npm = require('./npm.js')
const packument = require('libnpm/packument')
const path = require('path')
const readJson = require('libnpm/read-json')
const relativeDate = require('tiny-relative-date')
const semver = require('semver')
const style = require('ansistyles')
const usage = require('./utils/usage')
const util = require('util')
const validateName = require('validate-npm-package-name')

const ViewConfig = figgyPudding({
  global: {},
  json: {},
  tag: {},
  unicode: {}
})

view.usage = usage(
  'view',
  'npm view [<@scope>/]<pkg>[@<version>] [<field>[.subfield]...]'
)

view.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length <= 2) {
    // FIXME: there used to be registry completion here, but it stopped making
    // sense somewhere around 50,000 packages on the registry
    return cb()
  }
  // have the package, get the fields.
  const config = ViewConfig(npmConfig())
  const tag = config.tag
  const spec = npa(opts.conf.argv.remain[2])
  return packument(spec, config).then(d => {
    const dv = d.versions[d['dist-tags'][tag]]
    d.versions = Object.keys(d.versions).sort(semver.compareLoose)
    return getFields(d).concat(getFields(dv))
  }).nodeify(cb)

  function getFields (d, f, pref) {
    f = f || []
    if (!d) return f
    pref = pref || []
    Object.keys(d).forEach(function (k) {
      if (k.charAt(0) === '_' || k.indexOf('.') !== -1) return
      const p = pref.concat(k).join('.')
      f.push(p)
      if (Array.isArray(d[k])) {
        d[k].forEach(function (val, i) {
          const pi = p + '[' + i + ']'
          if (val && typeof val === 'object') getFields(val, f, [p])
          else f.push(pi)
        })
        return
      }
      if (typeof d[k] === 'object') getFields(d[k], f, [p])
    })
    return f
  }
}

function view (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  if (!args.length) args = ['.']

  const opts = ViewConfig(npmConfig())
  const pkg = args.shift()
  let nv
  if (/^[.]@/.test(pkg)) {
    nv = npa.resolve(null, pkg.slice(2))
  } else {
    nv = npa(pkg)
  }
  const name = nv.name
  const local = (name === '.' || !name)

  if (opts.global && local) {
    return cb(new Error('Cannot use view command in global mode.'))
  }

  if (local) {
    const dir = npm.prefix
    BB.resolve(readJson(path.resolve(dir, 'package.json'))).nodeify((er, d) => {
      d = d || {}
      if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
      if (!d.name) return cb(new Error('Invalid package.json'))

      const p = d.name
      nv = npa(p)
      if (pkg && ~pkg.indexOf('@')) {
        nv.rawSpec = pkg.split('@')[pkg.indexOf('@')]
      }

      fetchAndRead(nv, args, silent, opts, cb)
    })
  } else {
    fetchAndRead(nv, args, silent, opts, cb)
  }
}

function fetchAndRead (nv, args, silent, opts, cb) {
  // get the data about this package
  let version = nv.rawSpec || npm.config.get('tag')

  return packument(nv, opts.concat({
    fullMetadata: true,
    'prefer-online': true
  })).catch(err => {
    // TODO - this should probably go into pacote, but the tests expect it.
    if (err.code === 'E404') {
      err.message = `'${nv.name}' is not in the npm registry.`
      const validated = validateName(nv.name)
      if (!validated.validForNewPackages) {
        err.message += '\n'
        err.message += (validated.errors || []).join('\n')
        err.message += (validated.warnings || []).join('\n')
      } else {
        err.message += '\nYou should bug the author to publish it'
        err.message += '\n(or use the name yourself!)'
        err.message += '\n'
        err.message += '\nNote that you can also install from a'
        err.message += '\ntarball, folder, http url, or git url.'
      }
    }
    throw err
  }).then(data => {
    if (data['dist-tags'] && data['dist-tags'][version]) {
      version = data['dist-tags'][version]
    }

    if (data.time && data.time.unpublished) {
      const u = data.time.unpublished
      let er = new Error('Unpublished by ' + u.name + ' on ' + u.time)
      er.statusCode = 404
      er.code = 'E404'
      er.pkgid = data._id
      throw er
    }

    const results = []
    let error = null
    const versions = data.versions || {}
    data.versions = Object.keys(versions).sort(semver.compareLoose)
    if (!args.length) args = ['']

    // remove readme unless we asked for it
    if (args.indexOf('readme') === -1) {
      delete data.readme
    }

    Object.keys(versions).forEach(function (v) {
      if (semver.satisfies(v, version, true)) {
        args.forEach(function (args) {
          // remove readme unless we asked for it
          if (args.indexOf('readme') !== -1) {
            delete versions[v].readme
          }
          results.push(showFields(data, versions[v], args))
        })
      }
    })
    let retval = results.reduce(reducer, {})

    if (args.length === 1 && args[0] === '') {
      retval = cleanBlanks(retval)
      log.silly('view', retval)
    }

    if (silent) {
      return retval
    } else if (error) {
      throw error
    } else if (
      !opts.json &&
      args.length === 1 &&
      args[0] === ''
    ) {
      data.version = version
      return BB.all(
        results.map((v) => prettyView(data, v[Object.keys(v)[0]][''], opts))
      ).then(() => retval)
    } else {
      return BB.fromNode(cb => {
        printData(retval, data._id, opts, cb)
      }).then(() => retval)
    }
  }).nodeify(cb)
}

function prettyView (packument, manifest, opts) {
  // More modern, pretty printing of default view
  const unicode = opts.unicode
  return BB.try(() => {
    if (!manifest) {
      log.error(
        'view',
        'No matching versions.\n' +
        'To see a list of versions, run:\n' +
        `> npm view ${packument.name} versions`
      )
      return
    }
    const tags = []
    Object.keys(packument['dist-tags']).forEach((t) => {
      const version = packument['dist-tags'][t]
      tags.push(`${style.bright(color.green(t))}: ${version}`)
    })
    const unpackedSize = manifest.dist.unpackedSize &&
      byteSize(manifest.dist.unpackedSize)
    const licenseField = manifest.license || manifest.licence || 'Proprietary'
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
        email: color.cyan(manifest._npmUser.email)
      }),
      modified: packument.time ? color.yellow(relativeDate(packument.time[packument.version])) : undefined,
      maintainers: (packument.maintainers || []).map((u) => unparsePerson({
        name: color.yellow(u.name),
        email: color.cyan(u.email)
      })),
      repo: (
        manifest.bugs && (manifest.bugs.url || manifest.bugs)
      ) || (
        manifest.repository && (manifest.repository.url || manifest.repository)
      ),
      site: (
        manifest.homepage && (manifest.homepage.url || manifest.homepage)
      ),
      stars: color.yellow('' + packument.users ? Object.keys(packument.users || {}).length : 0),
      tags,
      tarball: color.cyan(manifest.dist.tarball),
      shasum: color.yellow(manifest.dist.shasum),
      integrity: manifest.dist.integrity && color.yellow(manifest.dist.integrity),
      fileCount: manifest.dist.fileCount && color.yellow(manifest.dist.fileCount),
      unpackedSize: unpackedSize && color.yellow(unpackedSize.value) + ' ' + unpackedSize.unit
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
      console.log(columns(info.deps.slice(0, maxDeps), {padding: 1}))
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
      if (info.modified) { publishInfo += ` ${info.modified}` }
      if (info.publisher) { publishInfo += ` by ${info.publisher}` }
      console.log('')
      console.log(publishInfo)
    }
  })
}

function cleanBlanks (obj) {
  const clean = {}
  Object.keys(obj).forEach(function (version) {
    clean[version] = obj[version]['']
  })
  return clean
}

function reducer (l, r) {
  if (r) {
    Object.keys(r).forEach(function (v) {
      l[v] = l[v] || {}
      Object.keys(r[v]).forEach(function (t) {
        l[v][t] = r[v][t]
      })
    })
  }

  return l
}

// return whatever was printed
function showFields (data, version, fields) {
  const o = {}
  ;[data, version].forEach(function (s) {
    Object.keys(s).forEach(function (k) {
      o[k] = s[k]
    })
  })
  return search(o, fields.split('.'), version.version, fields)
}

function search (data, fields, version, title) {
  let field
  const tail = fields
  while (!field && fields.length) field = tail.shift()
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
    if (data.field && data.field.hasOwnProperty(index)) {
      return search(data[field][index], tail, version, title)
    } else {
      field = field + '[' + index + ']'
    }
  }
  if (Array.isArray(data)) {
    if (data.length === 1) {
      return search(data[0], fields, version, title)
    }
    let results = []
    data.forEach(function (data, i) {
      const tl = title.length
      const newt = title.substr(0, tl - fields.join('.').length - 1) +
                 '[' + i + ']' + [''].concat(fields).join('.')
      results.push(search(data, fields.slice(), version, newt))
    })
    results = results.reduce(reducer, {})
    return results
  }
  if (!data.hasOwnProperty(field)) return undefined
  data = data[field]
  if (tail.length) {
    if (typeof data === 'object') {
      // there are more fields to deal with.
      return search(data, tail, version, title)
    } else {
      return new Error('Not an object: ' + data)
    }
  }
  o = {}
  o[version] = {}
  o[version][title] = data
  return o
}

function printData (data, name, opts, cb) {
  const versions = Object.keys(data)
  let msg = ''
  let msgJson = []
  const includeVersions = versions.length > 1
  let includeFields

  versions.forEach(function (v) {
    const fields = Object.keys(data[v])
    includeFields = includeFields || (fields.length > 1)
    if (opts.json) msgJson.push({})
    fields.forEach(function (f) {
      let d = cleanup(data[v][f])
      if (fields.length === 1 && opts.json) {
        msgJson[msgJson.length - 1][f] = d
      }
      if (includeVersions || includeFields || typeof d !== 'string') {
        if (opts.json) {
          msgJson[msgJson.length - 1][f] = d
        } else {
          d = util.inspect(d, { showHidden: false, depth: 5, colors: npm.color, maxArrayLength: null })
        }
      } else if (typeof d === 'string' && opts.json) {
        d = JSON.stringify(d)
      }
      if (!opts.json) {
        if (f && includeFields) f += ' = '
        if (d.indexOf('\n') !== -1) d = ' \n' + d
        msg += (includeVersions ? name + '@' + v + ' ' : '') +
               (includeFields ? f : '') + d + '\n'
      }
    })
  })

  if (opts.json) {
    if (msgJson.length && Object.keys(msgJson[0]).length === 1) {
      const k = Object.keys(msgJson[0])[0]
      msgJson = msgJson.map(function (m) { return m[k] })
    }

    if (msgJson.length === 1) {
      msg = JSON.stringify(msgJson[0], null, 2) + '\n'
    } else if (msgJson.length > 1) {
      msg = JSON.stringify(msgJson, null, 2) + '\n'
    }
  }

  // preserve output symmetry by adding a whitespace-only line at the end if
  // there's one at the beginning
  if (/^\s*\n/.test(msg)) msg += '\n'

  // disable the progress bar entirely, as we can't meaningfully update it if
  // we may have partial lines printed.
  log.disableProgress()

  // print directly to stdout to not unnecessarily add blank lines
  process.stdout.write(msg, () => cb(null, data))
}
function cleanup (data) {
  if (Array.isArray(data)) {
    return data.map(cleanup)
  }
  if (!data || typeof data !== 'object') return data

  if (typeof data.versions === 'object' &&
      data.versions &&
      !Array.isArray(data.versions)) {
    data.versions = Object.keys(data.versions || {})
  }

  let keys = Object.keys(data)
  keys.forEach(function (d) {
    if (d.charAt(0) === '_') delete data[d]
    else if (typeof data[d] === 'object') data[d] = cleanup(data[d])
  })
  keys = Object.keys(data)
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
  if (typeof d === 'string') return d
  return d.name +
    (d.email ? ' <' + d.email + '>' : '') +
    (d.url ? ' (' + d.url + ')' : '')
}
