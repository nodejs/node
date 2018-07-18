'use strict'

const Bluebird = require('bluebird')

const audit = require('./install/audit.js')
const fs = require('graceful-fs')
const Installer = require('./install.js').Installer
const lockVerify = require('lock-verify')
const log = require('npmlog')
const npa = require('npm-package-arg')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const parseJson = require('json-parse-better-errors')

const readFile = Bluebird.promisify(fs.readFile)

module.exports = auditCmd

const usage = require('./utils/usage')
auditCmd.usage = usage(
  'audit',
  '\nnpm audit [--json]' +
  '\nnpm audit fix ' +
  '[--force|--package-lock-only|--dry-run|--production|--only=(dev|prod)]'
)

auditCmd.completion = function (opts, cb) {
  const argv = opts.conf.argv.remain

  switch (argv[2]) {
    case 'audit':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

class Auditor extends Installer {
  constructor (where, dryrun, args, opts) {
    super(where, dryrun, args, opts)
    this.deepArgs = (opts && opts.deepArgs) || []
    this.runId = opts.runId || ''
    this.audit = false
  }

  loadAllDepsIntoIdealTree (cb) {
    Bluebird.fromNode(cb => super.loadAllDepsIntoIdealTree(cb)).then(() => {
      if (this.deepArgs && this.deepArgs.length) {
        this.deepArgs.forEach(arg => {
          arg.reduce((acc, child, ii) => {
            if (!acc) {
              // We might not always be able to find `target` through the given
              // path. If we can't we'll just ignore it.
              return
            }
            const spec = npa(child)
            const target = (
              acc.requires.find(n => n.package.name === spec.name) ||
              acc.requires.find(
                n => audit.scrub(n.package.name, this.runId) === spec.name
              )
            )
            if (target && ii === arg.length - 1) {
              target.loaded = false
              // This kills `hasModernMeta()` and forces a re-fetch
              target.package = {
                name: spec.name,
                version: spec.fetchSpec,
                _requested: target.package._requested
              }
              delete target.fakeChild
              let parent = target.parent
              while (parent) {
                parent.loaded = false
                parent = parent.parent
              }
              target.requiredBy.forEach(par => {
                par.loaded = false
                delete par.fakeChild
              })
            }
            return target
          }, this.idealTree)
        })
        return Bluebird.fromNode(cb => super.loadAllDepsIntoIdealTree(cb))
      }
    }).nodeify(cb)
  }

  // no top level lifecycles on audit
  runPreinstallTopLevelLifecycles (cb) { cb() }
  runPostinstallTopLevelLifecycles (cb) { cb() }
}

function maybeReadFile (name) {
  const file = `${npm.prefix}/${name}`
  return readFile(file)
    .then((data) => {
      try {
        return parseJson(data)
      } catch (ex) {
        ex.code = 'EJSONPARSE'
        throw ex
      }
    })
    .catch({code: 'ENOENT'}, () => null)
    .catch((ex) => {
      ex.file = file
      throw ex
    })
}

function filterEnv (action) {
  const includeDev = npm.config.get('dev') ||
    (!/^prod(uction)?$/.test(npm.config.get('only')) && !npm.config.get('production')) ||
    /^dev(elopment)?$/.test(npm.config.get('only')) ||
    /^dev(elopment)?$/.test(npm.config.get('also'))
  const includeProd = !/^dev(elopment)?$/.test(npm.config.get('only'))
  const resolves = action.resolves.filter(({dev}) => {
    return (dev && includeDev) || (!dev && includeProd)
  })
  if (resolves.length) {
    return Object.assign({}, action, {resolves})
  }
}

function auditCmd (args, cb) {
  if (npm.config.get('global')) {
    const err = new Error('`npm audit` does not support testing globals')
    err.code = 'EAUDITGLOBAL'
    throw err
  }
  if (args.length && args[0] !== 'fix') {
    return cb(new Error('Invalid audit subcommand: `' + args[0] + '`\n\nUsage:\n' + auditCmd.usage))
  }
  return Bluebird.all([
    maybeReadFile('npm-shrinkwrap.json'),
    maybeReadFile('package-lock.json'),
    maybeReadFile('package.json')
  ]).spread((shrinkwrap, lockfile, pkgJson) => {
    const sw = shrinkwrap || lockfile
    if (!pkgJson) {
      const err = new Error('No package.json found: Cannot audit a project without a package.json')
      err.code = 'EAUDITNOPJSON'
      throw err
    }
    if (!sw) {
      const err = new Error('Neither npm-shrinkwrap.json nor package-lock.json found: Cannot audit a project without a lockfile')
      err.code = 'EAUDITNOLOCK'
      throw err
    } else if (shrinkwrap && lockfile) {
      log.warn('audit', 'Both npm-shrinkwrap.json and package-lock.json exist, using npm-shrinkwrap.json.')
    }
    const requires = Object.assign(
      {},
      (pkgJson && pkgJson.dependencies) || {},
      (pkgJson && pkgJson.devDependencies) || {}
    )
    return lockVerify(npm.prefix).then((result) => {
      if (result.status) return audit.generate(sw, requires)

      const lockFile = shrinkwrap ? 'npm-shrinkwrap.json' : 'package-lock.json'
      const err = new Error(`Errors were found in your ${lockFile}, run  npm install  to fix them.\n    ` +
        result.errors.join('\n    '))
      err.code = 'ELOCKVERIFY'
      throw err
    })
  }).then((auditReport) => {
    return audit.submitForFullReport(auditReport)
  }).catch((err) => {
    if (err.statusCode === 404 || err.statusCode >= 500) {
      const ne = new Error(`Your configured registry (${npm.config.get('registry')}) does not support audit requests.`)
      ne.code = 'ENOAUDIT'
      ne.wrapped = err
      throw ne
    }
    throw err
  }).then((auditResult) => {
    if (args[0] === 'fix') {
      const actions = (auditResult.actions || []).reduce((acc, action) => {
        action = filterEnv(action)
        if (!action) { return acc }
        if (action.isMajor) {
          acc.major.add(`${action.module}@${action.target}`)
          action.resolves.forEach(({id, path}) => acc.majorFixes.add(`${id}::${path}`))
        } else if (action.action === 'install') {
          acc.install.add(`${action.module}@${action.target}`)
          action.resolves.forEach(({id, path}) => acc.installFixes.add(`${id}::${path}`))
        } else if (action.action === 'update') {
          const name = action.module
          const version = action.target
          action.resolves.forEach(vuln => {
            acc.updateFixes.add(`${vuln.id}::${vuln.path}`)
            const modPath = vuln.path.split('>')
            const newPath = modPath.slice(
              0, modPath.indexOf(name)
            ).concat(`${name}@${version}`)
            if (newPath.length === 1) {
              acc.install.add(newPath[0])
            } else {
              acc.update.add(newPath.join('>'))
            }
          })
        } else if (action.action === 'review') {
          action.resolves.forEach(({id, path}) => acc.review.add(`${id}::${path}`))
        }
        return acc
      }, {
        install: new Set(),
        installFixes: new Set(),
        update: new Set(),
        updateFixes: new Set(),
        major: new Set(),
        majorFixes: new Set(),
        review: new Set()
      })
      return Bluebird.try(() => {
        const installMajor = npm.config.get('force')
        const installCount = actions.install.size + (installMajor ? actions.major.size : 0) + actions.update.size
        const vulnFixCount = new Set([...actions.installFixes, ...actions.updateFixes, ...(installMajor ? actions.majorFixes : [])]).size
        const metavuln = auditResult.metadata.vulnerabilities
        const total = Object.keys(metavuln).reduce((acc, key) => acc + metavuln[key], 0)
        if (installCount) {
          log.verbose(
            'audit',
            'installing',
            [...actions.install, ...(installMajor ? actions.major : []), ...actions.update]
          )
        }
        return Bluebird.fromNode(cb => {
          new Auditor(
            npm.prefix,
            !!npm.config.get('dry-run'),
            [...actions.install, ...(installMajor ? actions.major : [])],
            {
              runId: auditResult.runId,
              deepArgs: [...actions.update].map(u => u.split('>'))
            }
          ).run(cb)
        }).then(() => {
          const numScanned = auditResult.metadata.totalDependencies
          if (!npm.config.get('json') && !npm.config.get('parseable')) {
            output(`fixed ${vulnFixCount} of ${total} vulnerabilit${total === 1 ? 'y' : 'ies'} in ${numScanned} scanned package${numScanned === 1 ? '' : 's'}`)
            if (actions.review.size) {
              output(`  ${actions.review.size} vulnerabilit${actions.review.size === 1 ? 'y' : 'ies'} required manual review and could not be updated`)
            }
            if (actions.major.size) {
              output(`  ${actions.major.size} package update${actions.major.size === 1 ? '' : 's'} for ${actions.majorFixes.size} vuln${actions.majorFixes.size === 1 ? '' : 's'} involved breaking changes`)
              if (installMajor) {
                output('  (installed due to `--force` option)')
              } else {
                output('  (use `npm audit fix --force` to install breaking changes; or do it by hand)')
              }
            }
          }
        })
      })
    } else {
      const vulns =
        auditResult.metadata.vulnerabilities.low +
        auditResult.metadata.vulnerabilities.moderate +
        auditResult.metadata.vulnerabilities.high +
        auditResult.metadata.vulnerabilities.critical
      if (vulns > 0) process.exitCode = 1
      if (npm.config.get('parseable')) {
        return audit.printParseableReport(auditResult)
      } else {
        return audit.printFullReport(auditResult)
      }
    }
  }).asCallback(cb)
}
