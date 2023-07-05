const fs = require('fs/promises')
const { glob } = require('glob')
const normalizePackageBin = require('npm-normalize-package-bin')
const normalizePackageData = require('normalize-package-data')
const path = require('path')
const log = require('proc-log')
const git = require('@npmcli/git')

const normalize = async (pkg, { strict, steps, root }) => {
  const data = pkg.content
  const scripts = data.scripts || {}
  const pkgId = `${data.name ?? ''}@${data.version ?? ''}`

  // remove attributes that start with "_"
  if (steps.includes('_attributes')) {
    for (const key in data) {
      if (key.startsWith('_')) {
        delete pkg.content[key]
      }
    }
  }

  // build the "_id" attribute
  if (steps.includes('_id')) {
    if (data.name && data.version) {
      data._id = pkgId
    }
  }

  // fix bundledDependencies typo
  if (steps.includes('bundledDependencies')) {
    if (data.bundleDependencies === undefined && data.bundledDependencies !== undefined) {
      data.bundleDependencies = data.bundledDependencies
    }
    delete data.bundledDependencies
  }
  // expand "bundleDependencies: true or translate from object"
  if (steps.includes('bundleDependencies')) {
    const bd = data.bundleDependencies
    if (bd === false && !steps.includes('bundleDependenciesDeleteFalse')) {
      data.bundleDependencies = []
    } else if (bd === true) {
      data.bundleDependencies = Object.keys(data.dependencies || {})
    } else if (bd && typeof bd === 'object') {
      if (!Array.isArray(bd)) {
        data.bundleDependencies = Object.keys(bd)
      }
    } else {
      delete data.bundleDependencies
    }
  }

  // it was once common practice to list deps both in optionalDependencies and
  // in dependencies, to support npm versions that did not know about
  // optionalDependencies.  This is no longer a relevant need, so duplicating
  // the deps in two places is unnecessary and excessive.
  if (steps.includes('optionalDedupe')) {
    if (data.dependencies &&
      data.optionalDependencies && typeof data.optionalDependencies === 'object') {
      for (const name in data.optionalDependencies) {
        delete data.dependencies[name]
      }
      if (!Object.keys(data.dependencies).length) {
        delete data.dependencies
      }
    }
  }

  // add "install" attribute if any "*.gyp" files exist
  if (steps.includes('gypfile')) {
    if (!scripts.install && !scripts.preinstall && data.gypfile !== false) {
      const files = await glob('*.gyp', { cwd: pkg.path })
      if (files.length) {
        scripts.install = 'node-gyp rebuild'
        data.scripts = scripts
        data.gypfile = true
      }
    }
  }

  // add "start" attribute if "server.js" exists
  if (steps.includes('serverjs') && !scripts.start) {
    try {
      await fs.access(path.join(pkg.path, 'server.js'))
      scripts.start = 'node server.js'
      data.scripts = scripts
    } catch {
      // do nothing
    }
  }

  // strip "node_modules/.bin" from scripts entries
  if (steps.includes('scripts') || steps.includes('scriptpath')) {
    const spre = /^(\.[/\\])?node_modules[/\\].bin[\\/]/
    if (typeof data.scripts === 'object') {
      for (const name in data.scripts) {
        if (typeof data.scripts[name] !== 'string') {
          delete data.scripts[name]
        } else if (steps.includes('scriptpath')) {
          data.scripts[name] = data.scripts[name].replace(spre, '')
        }
      }
    } else {
      delete data.scripts
    }
  }

  if (steps.includes('funding')) {
    if (data.funding && typeof data.funding === 'string') {
      data.funding = { url: data.funding }
    }
  }

  // populate "authors" attribute
  if (steps.includes('authors') && !data.contributors) {
    try {
      const authorData = await fs.readFile(path.join(pkg.path, 'AUTHORS'), 'utf8')
      const authors = authorData.split(/\r?\n/g)
        .map(line => line.replace(/^\s*#.*$/, '').trim())
        .filter(line => line)
      data.contributors = authors
    } catch {
      // do nothing
    }
  }

  // populate "readme" attribute
  if (steps.includes('readme') && !data.readme) {
    const mdre = /\.m?a?r?k?d?o?w?n?$/i
    const files = await glob('{README,README.*}', { cwd: pkg.path, nocase: true, mark: true })
    let readmeFile
    for (const file of files) {
      // don't accept directories.
      if (!file.endsWith(path.sep)) {
        if (file.match(mdre)) {
          readmeFile = file
          break
        }
        if (file.endsWith('README')) {
          readmeFile = file
        }
      }
    }
    if (readmeFile) {
      const readmeData = await fs.readFile(path.join(pkg.path, readmeFile), 'utf8')
      data.readme = readmeData
      data.readmeFilename = readmeFile
    }
  }

  // expand directories.man
  if (steps.includes('mans') && !data.man && data.directories?.man) {
    const manDir = data.directories.man
    const cwd = path.resolve(pkg.path, manDir)
    const files = await glob('**/*.[0-9]', { cwd })
    data.man = files.map(man =>
      path.relative(pkg.path, path.join(cwd, man)).split(path.sep).join('/')
    )
  }

  if (steps.includes('bin') || steps.includes('binDir') || steps.includes('binRefs')) {
    normalizePackageBin(data)
  }

  // expand "directories.bin"
  if (steps.includes('binDir') && data.directories?.bin && !data.bin) {
    const binsDir = path.resolve(pkg.path, path.join('.', path.join('/', data.directories.bin)))
    const bins = await glob('**', { cwd: binsDir })
    data.bin = bins.reduce((acc, binFile) => {
      if (binFile && !binFile.startsWith('.')) {
        const binName = path.basename(binFile)
        acc[binName] = path.join(data.directories.bin, binFile)
      }
      return acc
    }, {})
    // *sigh*
    normalizePackageBin(data)
  }

  // populate "gitHead" attribute
  if (steps.includes('gitHead') && !data.gitHead) {
    const gitRoot = await git.find({ cwd: pkg.path, root })
    let head
    if (gitRoot) {
      try {
        head = await fs.readFile(path.resolve(gitRoot, '.git/HEAD'), 'utf8')
      } catch (err) {
      // do nothing
      }
    }
    let headData
    if (head) {
      if (head.startsWith('ref: ')) {
        const headRef = head.replace(/^ref: /, '').trim()
        const headFile = path.resolve(gitRoot, '.git', headRef)
        try {
          headData = await fs.readFile(headFile, 'utf8')
          headData = headData.replace(/^ref: /, '').trim()
        } catch (err) {
          // do nothing
        }
        if (!headData) {
          const packFile = path.resolve(gitRoot, '.git/packed-refs')
          try {
            let refs = await fs.readFile(packFile, 'utf8')
            if (refs) {
              refs = refs.split('\n')
              for (let i = 0; i < refs.length; i++) {
                const match = refs[i].match(/^([0-9a-f]{40}) (.+)$/)
                if (match && match[2].trim() === headRef) {
                  headData = match[1]
                  break
                }
              }
            }
          } catch {
            // do nothing
          }
        }
      } else {
        headData = head.trim()
      }
    }
    if (headData) {
      data.gitHead = headData
    }
  }

  // populate "types" attribute
  if (steps.includes('fillTypes')) {
    const index = data.main || 'index.js'

    if (typeof index !== 'string') {
      throw new TypeError('The "main" attribute must be of type string.')
    }

    // TODO exports is much more complicated than this in verbose format
    // We need to support for instance

    // "exports": {
    //   ".": [
    //     {
    //       "default": "./lib/npm.js"
    //     },
    //     "./lib/npm.js"
    //   ],
    //   "./package.json": "./package.json"
    // },
    // as well as conditional exports

    // if (data.exports && typeof data.exports === 'string') {
    //   index = data.exports
    // }

    // if (data.exports && data.exports['.']) {
    //   index = data.exports['.']
    //   if (typeof index !== 'string') {
    //   }
    // }
    const extless = path.join(path.dirname(index), path.basename(index, path.extname(index)))
    const dts = `./${extless}.d.ts`
    const hasDTSFields = 'types' in data || 'typings' in data
    if (!hasDTSFields) {
      try {
        await fs.access(path.join(pkg.path, dts))
        data.types = dts.split(path.sep).join('/')
      } catch {
        // do nothing
      }
    }
  }

  // "normalizeData" from read-package-json
  if (steps.includes('normalizeData')) {
    normalizePackageData(data, strict)
  }

  // Warn if the bin references don't point to anything.  This might be better
  // in normalize-package-data if it had access to the file path.
  if (steps.includes('binRefs') && data.bin instanceof Object) {
    for (const key in data.bin) {
      try {
        await fs.access(path.resolve(pkg.path, data.bin[key]))
      } catch {
        log.warn('package-json', pkgId, `No bin file found at ${data.bin[key]}`)
        // XXX: should a future breaking change delete bin entries that cannot be accessed?
      }
    }
  }
}

module.exports = normalize
