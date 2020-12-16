
module.exports = help

help.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2)
    return cb(null, [])
  getSections(cb)
}

const npmUsage = require('./utils/npm-usage.js')
var path = require('path')
var spawn = require('./utils/spawn')
var npm = require('./npm.js')
var log = require('npmlog')
var openUrl = require('./utils/open-url')
var glob = require('glob')
var output = require('./utils/output.js')

const usage = require('./utils/usage.js')

help.usage = usage('help', 'npm help <term> [<terms..>]')

function help (args, cb) {
  var argv = npm.config.parsedArgv.cooked

  var argnum = 0
  if (args.length === 2 && ~~args[0])
    argnum = ~~args.shift()

  // npm help foo bar baz: search topics
  if (args.length > 1 && args[0])
    return npm.commands['help-search'](args, cb)

  const affordances = {
    'find-dupes': 'dedupe',
  }
  var section = affordances[args[0]] || npm.deref(args[0]) || args[0]

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

  var pref = [1, 5, 7]
  if (argnum) {
    pref = [argnum].concat(pref.filter(function (n) {
      return n !== argnum
    }))
  }

  // npm help <section>: Try to find the path
  var manroot = path.resolve(__dirname, '..', 'man')

  // legacy
  if (section === 'global')
    section = 'folders'
  else if (section.match(/.*json/))
    section = section.replace('.json', '-json')

  // find either /section.n or /npm-section.n
  // The glob is used in the glob.  The regexp is used much
  // further down.  Globs and regexps are different
  var compextglob = '.+(gz|bz2|lzma|[FYzZ]|xz)'
  var compextre = '\\.(gz|bz2|lzma|[FYzZ]|xz)$'
  var f = '+(npm-' + section + '|' + section + ').[0-9]?(' + compextglob + ')'
  return glob(manroot + '/*/' + f, function (er, mans) {
    if (er)
      return cb(er)

    if (!mans.length)
      return npm.commands['help-search'](args, cb)

    mans = mans.map(function (man) {
      var ext = path.extname(man)
      if (man.match(new RegExp(compextre)))
        man = path.basename(man, ext)

      return man
    })

    viewMan(pickMan(mans, pref), cb)
  })
}

function pickMan (mans, pref_) {
  var nre = /([0-9]+)$/
  var pref = {}
  pref_.forEach(function (sect, i) {
    pref[sect] = i
  })
  mans = mans.sort(function (a, b) {
    var an = a.match(nre)[1]
    var bn = b.match(nre)[1]
    return an === bn ? (a > b ? -1 : 1)
      : pref[an] < pref[bn] ? -1
      : 1
  })
  return mans[0]
}

function viewMan (man, cb) {
  var nre = /([0-9]+)$/
  var num = man.match(nre)[1]
  var section = path.basename(man, '.' + num)

  // at this point, we know that the specified man page exists
  var manpath = path.join(__dirname, '..', 'man')
  var env = {}
  Object.keys(process.env).forEach(function (i) {
    env[i] = process.env[i]
  })
  env.MANPATH = manpath
  var viewer = npm.config.get('viewer')

  var conf
  switch (viewer) {
    case 'woman':
      var a = ['-e', '(woman-find-file \'' + man + '\')']
      conf = { env: env, stdio: 'inherit' }
      var woman = spawn('emacsclient', a, conf)
      woman.on('close', cb)
      break

    case 'browser':
      try {
        var url = htmlMan(man)
      } catch (err) {
        return cb(err)
      }
      openUrl(url, 'help available at the following URL', cb)
      break

    default:
      conf = { env: env, stdio: 'inherit' }
      var manProcess = spawn('man', [num, section], conf)
      manProcess.on('close', cb)
      break
  }
}

function htmlMan (man) {
  var sect = +man.match(/([0-9]+)$/)[1]
  var f = path.basename(man).replace(/[.]([0-9]+)$/, '')
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
  var g = path.resolve(__dirname, '../man/man[0-9]/*.[0-9]')
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
