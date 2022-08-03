const { spawn } = require('child_process')
const path = require('path')
const openUrl = require('../utils/open-url.js')
const { promisify } = require('util')
const glob = promisify(require('glob'))
const localeCompare = require('@isaacs/string-locale-compare')('en')

const globify = pattern => pattern.split('\\').join('/')
const BaseCommand = require('../base-command.js')

// Strips out the number from foo.7 or foo.7. or foo.7.tgz
// We don't currently compress our man pages but if we ever did this would
// seemlessly continue supporting it
const manNumberRegex = /\.(\d+)(\.[^/\\]*)?$/
// Searches for the "npm-" prefix in page names, to prefer those.
const manNpmPrefixRegex = /\/npm-/

class Help extends BaseCommand {
  static description = 'Get help on npm'
  static name = 'help'
  static usage = ['<term> [<terms..>]']
  static params = ['viewer']
  static ignoreImplicitWorkspace = true

  async completion (opts) {
    if (opts.conf.argv.remain.length > 2) {
      return []
    }
    const g = path.resolve(__dirname, '../../man/man[0-9]/*.[0-9]')
    const files = await glob(globify(g))

    return Object.keys(files.reduce(function (acc, file) {
      file = path.basename(file).replace(/\.[0-9]+$/, '')
      file = file.replace(/^npm-/, '')
      acc[file] = true
      return acc
    }, { help: true }))
  }

  async exec (args) {
    // By default we search all of our man subdirectories, but if the user has
    // asked for a specific one we limit the search to just there
    let manSearch = 'man*'
    if (/^\d+$/.test(args[0])) {
      manSearch = `man${args.shift()}`
    }

    if (!args.length) {
      return this.npm.output(await this.npm.usage)
    }

    // npm help foo bar baz: search topics
    if (args.length > 1) {
      return this.helpSearch(args)
    }

    let section = this.npm.deref(args[0]) || args[0]

    // support `npm help package.json`
    section = section.replace('.json', '-json')

    const manroot = path.resolve(__dirname, '..', '..', 'man')
    // find either section.n or npm-section.n
    const f = `${manroot}/${manSearch}/?(npm-)${section}.[0-9]*`
    let mans = await glob(globify(f))
    mans = mans.sort((a, b) => {
      // Prefer the page with an npm prefix, if there's only one.
      const aHasPrefix = manNpmPrefixRegex.test(a)
      const bHasPrefix = manNpmPrefixRegex.test(b)
      if (aHasPrefix !== bHasPrefix) {
        return aHasPrefix ? -1 : 1
      }

      // Because the glob is (subtly) different from manNumberRegex,
      // we can't rely on it passing.
      const aManNumberMatch = a.match(manNumberRegex)
      const bManNumberMatch = b.match(manNumberRegex)
      if (aManNumberMatch) {
        if (!bManNumberMatch) {
          return -1
        }
        // man number sort first so that 1 aka commands are preferred
        if (aManNumberMatch[1] !== bManNumberMatch[1]) {
          return aManNumberMatch[1] - bManNumberMatch[1]
        }
      } else if (bManNumberMatch) {
        return 1
      }

      return localeCompare(a, b)
    })
    const man = mans[0]

    if (man) {
      await this.viewMan(man)
    } else {
      return this.helpSearch(args)
    }
  }

  helpSearch (args) {
    return this.npm.exec('help-search', args)
  }

  async viewMan (man) {
    const env = {}
    Object.keys(process.env).forEach(function (i) {
      env[i] = process.env[i]
    })
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
        await openUrl(this.npm, this.htmlMan(man), 'help available at the following URL', true)
        return

      default:
        args.push(man)
        break
    }

    const proc = spawn(bin, args, opts)
    return new Promise((resolve, reject) => {
      proc.on('exit', (code) => {
        if (code) {
          return reject(new Error(`help process exited with code: ${code}`))
        }

        return resolve()
      })
    })
  }

  // Returns the path to the html version of the man page
  htmlMan (man) {
    let sect = man.match(manNumberRegex)[1]
    const f = path.basename(man).replace(manNumberRegex, '')
    switch (sect) {
      case '1':
        sect = 'commands'
        break
      case '5':
        sect = 'configuring-npm'
        break
      case '7':
        sect = 'using-npm'
        break
    }
    return 'file:///' + path.resolve(__dirname, '..', '..', 'docs', 'output', sect, f + '.html')
  }
}
module.exports = Help
