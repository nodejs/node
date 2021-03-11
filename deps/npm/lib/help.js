const npmUsage = require('./utils/npm-usage.js')
const { spawn } = require('child_process')
const path = require('path')
const log = require('npmlog')
const openUrl = require('./utils/open-url.js')
const glob = require('glob')

const BaseCommand = require('./base-command.js')

class Help extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'help'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['<term> [<terms..>]']
  }

  async completion (opts) {
    if (opts.conf.argv.remain.length > 2)
      return []
    const g = path.resolve(__dirname, '../man/man[0-9]/*.[0-9]')
    const files = await new Promise((resolve, reject) => {
      glob(g, function (er, files) {
        if (er)
          return reject(er)
        resolve(files)
      })
    })

    return Object.keys(files.reduce(function (acc, file) {
      file = path.basename(file).replace(/\.[0-9]+$/, '')
      file = file.replace(/^npm-/, '')
      acc[file] = true
      return acc
    }, { help: true }))
  }

  exec (args, cb) {
    this.help(args).then(() => cb()).catch(cb)
  }

  async help (args) {
    const argv = this.npm.config.parsedArgv.cooked

    let argnum = 0
    if (args.length === 2 && ~~args[0])
      argnum = ~~args.shift()

    // npm help foo bar baz: search topics
    if (args.length > 1 && args[0])
      return this.helpSearch(args)

    const affordances = {
      'find-dupes': 'dedupe',
    }
    let section = affordances[args[0]] || this.npm.deref(args[0]) || args[0]

    // npm help <noargs>:  show basic usage
    if (!section) {
      npmUsage(this.npm, argv[0] === 'help')
      return
    }

    // npm <command> -h: show command usage
    if (this.npm.config.get('usage') &&
      this.npm.commands[section] &&
      this.npm.commands[section].usage) {
      this.npm.config.set('loglevel', 'silent')
      log.level = 'silent'
      this.npm.output(this.npm.commands[section].usage)
      return
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
    return new Promise((resolve, reject) => {
      glob(manroot + '/*/' + f, async (er, mans) => {
        if (er)
          return reject(er)

        if (!mans.length) {
          this.helpSearch(args).then(resolve).catch(reject)
          return
        }

        mans = mans.map((man) => {
          const ext = path.extname(man)
          if (man.match(new RegExp(compextre)))
            man = path.basename(man, ext)

          return man
        })

        this.viewMan(this.pickMan(mans, pref), (err) => {
          if (err)
            return reject(err)
          return resolve()
        })
      })
    })
  }

  helpSearch (args) {
    return new Promise((resolve, reject) => {
      this.npm.commands['help-search'](args, (err) => {
        // This would only error if args was empty, which it never is
        /* istanbul ignore next */
        if (err)
          return reject(err)

        resolve()
      })
    })
  }

  pickMan (mans, pref_) {
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

  viewMan (man, cb) {
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
    const viewer = this.npm.config.get('viewer')

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
          const url = this.htmlMan(man)
          openUrl(this.npm, url, 'help available at the following URL').then(
            () => cb()
          ).catch(cb)
        } catch (err) {
          cb(err)
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

  htmlMan (man) {
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
}
module.exports = Help
