'use strict'

// Do a two-pass walk, first to get the list of packages that need to be
// bundled, then again to get the actual files and folders.
// Keep a cache of node_modules content and package.json data, so that the
// second walk doesn't have to re-do all the same work.

const bundleWalk = require('npm-bundled')
const BundleWalker = bundleWalk.BundleWalker

const ignoreWalk = require('ignore-walk')
const IgnoreWalker = ignoreWalk.Walker

const rootBuiltinRules = Symbol('root-builtin-rules')
const packageNecessaryRules = Symbol('package-necessary-rules')
const path = require('path')

const normalizePackageBin = require('npm-normalize-package-bin')

// Weird side-effect of this: a readme (etc) file will be included
// if it exists anywhere within a folder with a package.json file.
// The original intent was only to include these files in the root,
// but now users in the wild are dependent on that behavior for
// localized documentation and other use cases.  Adding a `/` to
// these rules, while tempting and arguably more "correct", is a
// significant change that will break existing use cases.
const packageMustHaveFileNames = 'readme|copying|license|licence'

const packageMustHaves = `@(${packageMustHaveFileNames}){,.*[^~$]}`
const packageMustHavesRE = new RegExp(`^(${packageMustHaveFileNames})(\\..*[^~$])?$`, 'i')

const fs = require('fs')
const glob = require('glob')
const globify = pattern => pattern.split('\\').join('/')

const pathHasPkg = (input) => {
  if (!input.startsWith('node_modules/')) {
    return false
  }

  const segments = input.slice('node_modules/'.length).split('/', 2)
  return segments[0].startsWith('@')
    ? segments.length === 2
    : true
}

const pkgFromPath = (input) => {
  const segments = input.slice('node_modules/'.length).split('/', 2)
  return segments[0].startsWith('@')
    ? segments.join('/')
    : segments[0]
}

const defaultRules = [
  '.npmignore',
  '.gitignore',
  '**/.git',
  '**/.svn',
  '**/.hg',
  '**/CVS',
  '**/.git/**',
  '**/.svn/**',
  '**/.hg/**',
  '**/CVS/**',
  '/.lock-wscript',
  '/.wafpickle-*',
  '/build/config.gypi',
  'npm-debug.log',
  '**/.npmrc',
  '.*.swp',
  '.DS_Store',
  '**/.DS_Store/**',
  '._*',
  '**/._*/**',
  '*.orig',
  '/package-lock.json',
  '/yarn.lock',
  '/pnpm-lock.yaml',
  '/archived-packages/**',
]

// There may be others, but :?|<> are handled by node-tar
const nameIsBadForWindows = file => /\*/.test(file)

class Walker extends IgnoreWalker {
  constructor (opt) {
    opt = opt || {}

    // the order in which rules are applied.
    opt.ignoreFiles = [
      rootBuiltinRules,
      'package.json',
      '.npmignore',
      '.gitignore',
      packageNecessaryRules,
    ]

    opt.includeEmpty = false
    opt.path = opt.path || process.cwd()

    // only follow links in the root node_modules folder, because if those
    // folders are included, it's because they're bundled, and bundles
    // should include the contents, not the symlinks themselves.
    // This regexp tests to see that we're either a node_modules folder,
    // or a @scope within a node_modules folder, in the root's node_modules
    // hierarchy (ie, not in test/foo/node_modules/ or something).
    const followRe = /^(?:\/node_modules\/(?:@[^/]+\/[^/]+|[^/]+)\/)*\/node_modules(?:\/@[^/]+)?$/
    const rootPath = opt.parent ? opt.parent.root : opt.path
    const followTestPath = opt.path.replace(/\\/g, '/').slice(rootPath.length)
    opt.follow = followRe.test(followTestPath)

    super(opt)

    // ignore a bunch of things by default at the root level.
    // also ignore anything in the main project node_modules hierarchy,
    // except bundled dependencies
    if (this.isProject) {
      this.bundled = opt.bundled || []
      this.bundledScopes = Array.from(new Set(
        this.bundled.filter(f => /^@/.test(f))
          .map(f => f.split('/')[0])))
      const rules = defaultRules.join('\n') + '\n'
      this.packageJsonCache = this.parent ? this.parent.packageJsonCache
        : (opt.packageJsonCache || new Map())
      super.onReadIgnoreFile(rootBuiltinRules, rules, _ => _)
    } else {
      this.bundled = []
      this.bundledScopes = []
      this.packageJsonCache = this.parent.packageJsonCache
    }
  }

  get isProject () {
    return !this.parent || this.parent.follow && this.isSymbolicLink
  }

  onReaddir (entries) {
    if (this.isProject) {
      entries = entries.filter(e =>
        e !== '.git' &&
        !(e === 'node_modules' && this.bundled.length === 0)
      )
    }

    // if we have a package.json, then look in it for 'files'
    // we _only_ do this in the root project, not bundled deps
    // or other random folders.  Bundled deps are always assumed
    // to be in the state the user wants to include them, and
    // a package.json somewhere else might be a template or
    // test or something else entirely.
    if (!this.isProject || !entries.includes('package.json')) {
      return super.onReaddir(entries)
    }

    // when the cache has been seeded with the root manifest,
    // we must respect that (it may differ from the filesystem)
    const ig = path.resolve(this.path, 'package.json')

    if (this.packageJsonCache.has(ig)) {
      const pkg = this.packageJsonCache.get(ig)

      // fall back to filesystem when seeded manifest is invalid
      if (!pkg || typeof pkg !== 'object') {
        return this.readPackageJson(entries)
      }

      // feels wonky, but this ensures package bin is _always_
      // normalized, as well as guarding against invalid JSON
      return this.getPackageFiles(entries, JSON.stringify(pkg))
    }

    this.readPackageJson(entries)
  }

  onReadPackageJson (entries, er, pkg) {
    if (er) {
      this.emit('error', er)
    } else {
      this.getPackageFiles(entries, pkg)
    }
  }

  mustHaveFilesFromPackage (pkg) {
    const files = []
    if (pkg.browser) {
      files.push('/' + pkg.browser)
    }
    if (pkg.main) {
      files.push('/' + pkg.main)
    }
    if (pkg.bin) {
      // always an object because normalized already
      for (const key in pkg.bin) {
        files.push('/' + pkg.bin[key])
      }
    }
    files.push(
      '/package.json',
      '/npm-shrinkwrap.json',
      '!/package-lock.json',
      packageMustHaves
    )
    return files
  }

  getPackageFiles (entries, pkg) {
    try {
      // XXX this could be changed to use read-package-json-fast
      // which handles the normalizing of bins for us, and simplifies
      // the test for bundleDependencies and bundledDependencies later.
      // HOWEVER if we do this, we need to be sure that we're careful
      // about what we write back out since rpj-fast removes some fields
      // that the user likely wants to keep. it also would add a second
      // file read that we would want to optimize away.
      pkg = normalizePackageBin(JSON.parse(pkg.toString()))
    } catch (er) {
      // not actually a valid package.json
      return super.onReaddir(entries)
    }

    const ig = path.resolve(this.path, 'package.json')
    this.packageJsonCache.set(ig, pkg)

    // no files list, just return the normal readdir() result
    if (!Array.isArray(pkg.files)) {
      return super.onReaddir(entries)
    }

    pkg.files.push(...this.mustHaveFilesFromPackage(pkg))

    // If the package has a files list, then it's unlikely to include
    // node_modules, because why would you do that?  but since we use
    // the files list as the effective readdir result, that means it
    // looks like we don't have a node_modules folder at all unless we
    // include it here.
    if ((pkg.bundleDependencies || pkg.bundledDependencies) && entries.includes('node_modules')) {
      pkg.files.push('node_modules')
    }

    const patterns = Array.from(new Set(pkg.files)).reduce((set, pattern) => {
      const excl = pattern.match(/^!+/)
      if (excl) {
        pattern = pattern.slice(excl[0].length)
      }
      // strip off any / from the start of the pattern.  /foo => foo
      pattern = pattern.replace(/^\/+/, '')
      // an odd number of ! means a negated pattern.  !!foo ==> foo
      const negate = excl && excl[0].length % 2 === 1
      set.push({ pattern, negate })
      return set
    }, [])

    let n = patterns.length
    const set = new Set()
    const negates = new Set()
    const results = []
    const then = (pattern, negate, er, fileList, i) => {
      if (er) {
        return this.emit('error', er)
      }

      results[i] = { negate, fileList }
      if (--n === 0) {
        processResults(results)
      }
    }
    const processResults = processed => {
      for (const { negate, fileList } of processed) {
        if (negate) {
          fileList.forEach(f => {
            f = f.replace(/\/+$/, '')
            set.delete(f)
            negates.add(f)
          })
        } else {
          fileList.forEach(f => {
            f = f.replace(/\/+$/, '')
            set.add(f)
            negates.delete(f)
          })
        }
      }

      const list = Array.from(set)
      // replace the files array with our computed explicit set
      pkg.files = list.concat(Array.from(negates).map(f => '!' + f))
      const rdResult = Array.from(new Set(
        list.map(f => f.replace(/^\/+/, ''))
      ))
      super.onReaddir(rdResult)
    }

    // maintain the index so that we process them in-order only once all
    // are completed, otherwise the parallelism messes things up, since a
    // glob like **/*.js will always be slower than a subsequent !foo.js
    patterns.forEach(({ pattern, negate }, i) =>
      this.globFiles(pattern, (er, res) => then(pattern, negate, er, res, i)))
  }

  filterEntry (entry, partial) {
    // get the partial path from the root of the walk
    const p = this.path.slice(this.root.length + 1)
    const { isProject } = this
    const pkg = isProject && pathHasPkg(entry)
      ? pkgFromPath(entry)
      : null
    const rootNM = isProject && entry === 'node_modules'
    const rootPJ = isProject && entry === 'package.json'

    return (
      // if we're in a bundled package, check with the parent.
      /^node_modules($|\/)/i.test(p) && !this.isProject ? this.parent.filterEntry(
        this.basename + '/' + entry, partial)

      // if package is bundled, all files included
      // also include @scope dirs for bundled scoped deps
      // they'll be ignored if no files end up in them.
      // However, this only matters if we're in the root.
      // node_modules folders elsewhere, like lib/node_modules,
      // should be included normally unless ignored.
      : pkg ? this.bundled.indexOf(pkg) !== -1 ||
        this.bundledScopes.indexOf(pkg) !== -1

      // only walk top node_modules if we want to bundle something
      : rootNM ? !!this.bundled.length

      // always include package.json at the root.
      : rootPJ ? true

      // always include readmes etc in any included dir
      : packageMustHavesRE.test(entry) ? true

      // npm-shrinkwrap and package.json always included in the root pkg
      : isProject && (entry === 'npm-shrinkwrap.json' || entry === 'package.json')
        ? true

      // package-lock never included
        : isProject && entry === 'package-lock.json' ? false

        // otherwise, follow ignore-walk's logic
        : super.filterEntry(entry, partial)
    )
  }

  filterEntries () {
    if (this.ignoreRules['.npmignore']) {
      this.ignoreRules['.gitignore'] = null
    }
    this.filterEntries = super.filterEntries
    super.filterEntries()
  }

  addIgnoreFile (file, then) {
    const ig = path.resolve(this.path, file)
    if (file === 'package.json' && !this.isProject) {
      then()
    } else if (this.packageJsonCache.has(ig)) {
      this.onPackageJson(ig, this.packageJsonCache.get(ig), then)
    } else {
      super.addIgnoreFile(file, then)
    }
  }

  onPackageJson (ig, pkg, then) {
    this.packageJsonCache.set(ig, pkg)

    if (Array.isArray(pkg.files)) {
      // in this case we already included all the must-haves
      super.onReadIgnoreFile('package.json', pkg.files.map(
        f => '!' + f
      ).join('\n') + '\n', then)
    } else {
      // if there's a bin, browser or main, make sure we don't ignore it
      // also, don't ignore the package.json itself, or any files that
      // must be included in the package.
      const rules = this.mustHaveFilesFromPackage(pkg).map(f => `!${f}`)
      const data = rules.join('\n') + '\n'
      super.onReadIgnoreFile(packageNecessaryRules, data, then)
    }
  }

  // override parent stat function to completely skip any filenames
  // that will break windows entirely.
  // XXX(isaacs) Next major version should make this an error instead.
  stat ({ entry, file, dir }, then) {
    if (nameIsBadForWindows(entry)) {
      then()
    } else {
      super.stat({ entry, file, dir }, then)
    }
  }

  // override parent onstat function to nix all symlinks, other than
  // those coming out of the followed bundled symlink deps
  onstat ({ st, entry, file, dir, isSymbolicLink }, then) {
    if (st.isSymbolicLink()) {
      then()
    } else {
      super.onstat({ st, entry, file, dir, isSymbolicLink }, then)
    }
  }

  onReadIgnoreFile (file, data, then) {
    if (file === 'package.json') {
      try {
        const ig = path.resolve(this.path, file)
        this.onPackageJson(ig, JSON.parse(data), then)
      } catch (er) {
        // ignore package.json files that are not json
        then()
      }
    } else {
      super.onReadIgnoreFile(file, data, then)
    }
  }

  sort (a, b) {
    // optimize for compressibility
    // extname, then basename, then locale alphabetically
    // https://twitter.com/isntitvacant/status/1131094910923231232
    const exta = path.extname(a).toLowerCase()
    const extb = path.extname(b).toLowerCase()
    const basea = path.basename(a).toLowerCase()
    const baseb = path.basename(b).toLowerCase()

    return exta.localeCompare(extb, 'en') ||
      basea.localeCompare(baseb, 'en') ||
      a.localeCompare(b, 'en')
  }

  globFiles (pattern, cb) {
    glob(globify(pattern), { dot: true, cwd: this.path, nocase: true }, cb)
  }

  readPackageJson (entries) {
    fs.readFile(this.path + '/package.json', (er, pkg) =>
      this.onReadPackageJson(entries, er, pkg))
  }

  walker (entry, opt, then) {
    new Walker(this.walkerOpt(entry, opt)).on('done', then).start()
  }
}

const walk = (options, callback) => {
  options = options || {}
  const p = new Promise((resolve, reject) => {
    const bw = new BundleWalker(options)
    bw.on('done', bundled => {
      options.bundled = bundled
      options.packageJsonCache = bw.packageJsonCache
      new Walker(options).on('done', resolve).on('error', reject).start()
    })
    bw.start()
  })
  return callback ? p.then(res => callback(null, res), callback) : p
}

module.exports = walk
walk.Walker = Walker
