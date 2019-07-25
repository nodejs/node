'use strict'

const Bluebird = require('bluebird')

const audit = require('./install/audit.js')
const figgyPudding = require('figgy-pudding')
const fs = require('graceful-fs')
const Installer = require('./install.js').Installer
const lockVerify = require('lock-verify')
const log = require('npmlog')
const npa = require('libnpm/parse-arg')
const npm = require('./npm.js')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const parseJson = require('json-parse-better-errors')

const readFile = Bluebird.promisify(fs.readFile)

const AuditConfig = figgyPudding({
  also: {},
  'audit-level': {},
  deepArgs: 'deep-args',
  'deep-args': {},
  dev: {},
  force: {},
  'dry-run': {},
  global: {},
  json: {},
  only: {},
  parseable: {},
  prod: {},
  production: {},
  registry: {},
  runId: {}
})

module.exports = auditCmd

const usage = require('./utils/usage')
auditCmd.usage = usage(
  'audit',
  '\nnpm audit [--json] [--production]' +
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

function filterEnv (action, opts) {
  const includeDev = opts.dev ||
    (!/^prod(uction)?$/.test(opts.only) && !opts.production) ||
    /^dev(elopment)?$/.test(opts.only) ||
    /^dev(elopment)?$/.test(opts.also)
  const includeProd = !/^dev(elopment)?$/.test(opts.only)
  const resolves = action.resolves.filter(({dev}) => {
    return (dev && includeDev) || (!dev && includeProd)
  })
  if (resolves.length) {
    return Object.assign({}, action, {resolves})
  }
}

function auditCmd (args, cb) {
  const opts = AuditConfig(npmConfig())
  if (opts.global) {
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
      (!opts.production && pkgJson && pkgJson.devDependencies) || {}
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
    if (err.statusCode >= 400) {
      let msg
      if (err.statusCode === 401) {
        msg = `Either your login credentials are invalid or your registry (${opts.registry}) does not support audit.`
      } else if (err.statusCode === 404) {
        msg = `Your configured registry (${opts.registry}) does not support audit requests.`
      } else {
        msg = `Your configured registry (${opts.registry}) may not support audit requests, or the audit endpoint may be temporarily unavailable.`
      }
      if (err.body.length) {
        msg += '\nThe server said: ' + err.body
      }
      const ne = new Error(msg)
      ne.code = 'ENOAUDIT'
      ne.wrapped = err
      throw ne
    }
    throw err
  }).then((auditResult) => {
    if (args[0] === 'fix') {
      const actions = (auditResult.actions || []).reduce((acc, action) => {
        action = filterEnv(action, opts)
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
        const installMajor = opts.force
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
            !!opts['dry-run'],
            [...actions.install, ...(installMajor ? actions.major : [])],
            opts.concat({
              runId: auditResult.runId,
              deepArgs: [...actions.update].map(u => u.split('>'))
            }).toJSON()
          ).run(cb)
        }).then(() => {
          const numScanned = auditResult.metadata.totalDependencies
          if (!opts.json && !opts.parseable) {
            output(`fixed ${vulnFixCount} of ${total} vulnerabilit${total === 1 ? 'y' : 'ies'} in ${numScanned} scanned package${numScanned === 1 ? '' : 's'}`)
            if (actions.review.size) {
              output(`  ${actions.review.size} vulnerabilit${actions.review.size === 1 ? 'y' : 'ies'} required manual review and could not be updated`)
            }
            if (actions.major.size) {
              output(`  ${actions.major.size} package update${actions.major.size === 1 ? '' : 's'} for ${actions.majorFixes.size} vuln${actions.majorFixes.size === 1 ? '' : 's'} involved breaking changes`)
              if (installMajor) {
                output('  (installed due to `--force` option)')
              } else {
                output('  (use `npm audit fix --force` to install breaking changes;' +
                       ' or refer to `npm audit` for steps to fix these manually)')
              }
            }
          }
        })
      })
    } else {
      const levels = ['low', 'moderate', 'high', 'critical']
      const minLevel = levels.indexOf(opts['audit-level'])
      const vulns = levels.reduce((count, level, i) => {
        return i < minLevel ? count : count + (auditResult.metadata.vulnerabilities[level] || 0)
      }, 0)
      if (vulns > 0) process.exitCode = 1
      if (opts.parseable) {
        return audit.printParseableReport(auditResult)
      } else {
        return audit.printFullReport(auditResult)
      }
    }
  }).asCallback(cb)
}
