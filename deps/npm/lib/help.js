
module.exports = help

help.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2)
    return cb(null, [])
  getSections(cb)
}

const npmUsage = require('./utils/npm-usage.js')
const { spawn } = require('child_process')
const path = require('path')
const npm = require('./npm.js')
const log = require('npmlog')
const openUrl = require('./utils/open-url')
const glob = require('glob')
const output = require('./utils/output.js')

const usage = require('./utils/usage.js')

help.usage = usage('help', 'npm help <term> [<terms..>]')

function help (args, cb) {
  const argv = npm.config.parsedArgv.cooked

  let argnum = 0
  if (args.length === 2 && ~~args[0])
    argnum = ~~args.shift()

  // npm help foo bar baz: search topics
  if (args.length > 1 && args[0])
    return npm.commands['help-search'](args, cb)

  const affordances = {
    'find-dupes': 'dedupe',
  }
  let section = affordances[args[0]] || npm.deref(args[0]) || args[0]

  // npm help <noargs>:  show basic usage
  if (!section) {
    npmUsage(argv[0] === 'help')
    return cb()
  }

  // npm <command> -h: show command usage
  if (npm.config.get('usage') &&
      npm.commands[section] &&
      npm.commands[section].usage) {
    npm.config.set('loglevel', 'silent')
    log.level = 'silent'
    output(npm.commands[section].usage)
    return cb()
  }

  let pref = [1, 5, 7]
  if (argnum)
    pref = [argnum].concat(pref.filter(n => n !== argnum))

  // npm help <section>: Try to find the path
  const manroot = path.resolve(__dirname, '..', 'man')

  // legacy
  if (section === 'global')
    section = 'folders'
  else if (section.match(/.*json/))
    section = section.replace('.json', '-json')

  // find either /section.n or /npm-section.n
  // The glob is used in the glob.  The regexp is used much
  // further down.  Globs and regexps are different
  const compextglob = '.+(gz|bz2|lzma|[FYzZ]|xz)'
  const compextre = '\\.(gz|bz2|lzma|[FYzZ]|xz)$'
  const f = '+(npm-' + section + '|' + section + ').[0-9]?(' + compextglob + ')'
  return glob(manroot + '/*/' + f, (er, mans) => {
    if (er)
      return cb(er)

    if (!mans.length)
      return npm.commands['help-search'](args, cb)

    mans = mans.map((man) => {
      const ext = path.extname(man)
      if (man.match(new RegExp(compextre)))
        man = path.basename(man, ext)

      return man
    })

    viewMan(pickMan(mans, pref), cb)
  })
}

function pickMan (mans, pref_) {
  const nre = /([0-9]+)$/
  const pref = {}
  pref_.forEach((sect, i) => pref[sect] = i)
  mans = mans.sort((a, b) => {
    const an = a.match(nre)[1]
    const bn = b.match(nre)[1]
    return an === bn ? (a > b ? -1 : 1)
      : pref[an] < pref[bn] ? -1
      : 1
  })
  return mans[0]
}

function viewMan (man, cb) {
  const nre = /([0-9]+)$/
  const num = man.match(nre)[1]
  const section = path.basename(man, '.' + num)

  // at this point, we know that the specified man page exists
  const manpath = path.join(__dirname, '..', 'man')
  const env = {}
  Object.keys(process.env).forEach(function (i) {
    env[i] = process.env[i]
  })
  env.MANPATH = manpath
  const viewer = npm.config.get('viewer')

  const opts = {
    env,
    stdio: 'inherit',
  }

  let bin = 'man'
  const args = []
  switch (viewer) {
    case 'woman':
      bin = 'emacsclient'
      args.push('-e', `(woman-find-file '${man}')`)
      break

    case 'browser':
      bin = false
      try {
        const url = htmlMan(man)
        openUrl(url, 'help available at the following URL', cb)
      } catch (err) {
        return cb(err)
      }
      break

    default:
      args.push(num, section)
      break
  }

  if (bin) {
    const proc = spawn(bin, args, opts)
    proc.on('exit', (code) => {
      if (code)
        return cb(new Error(`help process exited with code: ${code}`))

      return cb()
    })
  }
}

function htmlMan (man) {
  let sect = +man.match(/([0-9]+)$/)[1]
  const f = path.basename(man).replace(/[.]([0-9]+)$/, '')
  switch (sect) {
    case 1:
      sect = 'commands'
      break
    case 5:
      sect = 'configuring-npm'
      break
    case 7:
      sect = 'using-npm'
      break
    default:
      throw new Error('invalid man section: ' + sect)
  }
  return 'file://' + path.resolve(__dirname, '..', 'docs', 'output', sect, f + '.html')
}

function getSections (cb) {
  const g = path.resolve(__dirname, '../man/man[0-9]/*.[0-9]')
  glob(g, function (er, files) {
    if (er)
      return cb(er)

    cb(null, Object.keys(files.reduce(function (acc, file) {
      file = path.basename(file).replace(/\.[0-9]+$/, '')
      file = file.replace(/^npm-/, '')
      acc[file] = true
      return acc
    }, { help: true })))
  })
}
