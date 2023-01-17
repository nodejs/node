const spawn = require('@npmcli/promise-spawn')
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
// hardcoded names for mansections
// XXX: these are used in the docs workspace and should be exported
// from npm so section names can changed more easily
const manSectionNames = {
  1: 'commands',
  5: 'configuring-npm',
  7: 'using-npm',
}

class Help extends BaseCommand {
  static description = 'Get help on npm'
  static name = 'help'
  static usage = ['<term> [<terms..>]']
  static params = ['viewer']

  async completion (opts) {
    if (opts.conf.argv.remain.length > 2) {
      return []
    }
    const g = path.resolve(this.npm.npmRoot, 'man/man[0-9]/*.[0-9]')
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
    const manSearch = /^\d+$/.test(args[0]) ? `man${args.shift()}` : 'man*'

    if (!args.length) {
      return this.npm.output(await this.npm.usage)
    }

    // npm help foo bar baz: search topics
    if (args.length > 1) {
      return this.helpSearch(args)
    }

    // `npm help package.json`
    const arg = (this.npm.deref(args[0]) || args[0]).replace('.json', '-json')

    // find either section.n or npm-section.n
    const f = globify(path.resolve(this.npm.npmRoot, `man/${manSearch}/?(npm-)${arg}.[0-9]*`))

    const [man] = await glob(f).then(r => r.sort((a, b) => {
      // Prefer the page with an npm prefix, if there's only one.
      const aHasPrefix = manNpmPrefixRegex.test(a)
      const bHasPrefix = manNpmPrefixRegex.test(b)
      if (aHasPrefix !== bHasPrefix) {
        /* istanbul ignore next */
        return aHasPrefix ? -1 : 1
      }

      // Because the glob is (subtly) different from manNumberRegex,
      // we can't rely on it passing.
      const aManNumberMatch = a.match(manNumberRegex)
      const bManNumberMatch = b.match(manNumberRegex)
      if (aManNumberMatch) {
        /* istanbul ignore next */
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
    }))

    return man ? this.viewMan(man) : this.helpSearch(args)
  }

  helpSearch (args) {
    return this.npm.exec('help-search', args)
  }

  async viewMan (man) {
    const viewer = this.npm.config.get('viewer')

    if (viewer === 'browser') {
      return openUrl(this.npm, this.htmlMan(man), 'help available at the following URL', true)
    }

    let args = ['man', [man]]
    if (viewer === 'woman') {
      args = ['emacsclient', ['-e', `(woman-find-file '${man}')`]]
    }

    return spawn(...args, { stdio: 'inherit' }).catch(err => {
      if (err.code) {
        throw new Error(`help process exited with code: ${err.code}`)
      } else {
        throw err
      }
    })
  }

  // Returns the path to the html version of the man page
  htmlMan (man) {
    const sect = manSectionNames[man.match(manNumberRegex)[1]]
    const f = path.basename(man).replace(manNumberRegex, '')
    return 'file:///' + path.resolve(this.npm.npmRoot, `docs/output/${sect}/${f}.html`)
  }
}
module.exports = Help
