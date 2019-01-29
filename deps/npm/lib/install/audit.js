'use strict'
exports.generate = generate
exports.generateFromInstall = generateFromInstall
exports.submitForInstallReport = submitForInstallReport
exports.submitForFullReport = submitForFullReport
exports.printInstallReport = printInstallReport
exports.printParseableReport = printParseableReport
exports.printFullReport = printFullReport

const auditReport = require('npm-audit-report')
const npmConfig = require('../config/figgy-config.js')
const figgyPudding = require('figgy-pudding')
const treeToShrinkwrap = require('../shrinkwrap.js').treeToShrinkwrap
const packageId = require('../utils/package-id.js')
const output = require('../utils/output.js')
const npm = require('../npm.js')
const qw = require('qw')
const regFetch = require('npm-registry-fetch')
const perf = require('../utils/perf.js')
const npa = require('npm-package-arg')
const uuid = require('uuid')
const ssri = require('ssri')
const cloneDeep = require('lodash.clonedeep')

// used when scrubbing module names/specifiers
const runId = uuid.v4()

const InstallAuditConfig = figgyPudding({
  color: {},
  json: {},
  unicode: {}
}, {
  other (key) {
    return /:registry$/.test(key)
  }
})

function submitForInstallReport (auditData) {
  const opts = InstallAuditConfig(npmConfig())
  const scopedRegistries = [...opts.keys()].filter(
    k => /:registry$/.test(k)
  ).map(k => opts[k])
  scopedRegistries.forEach(registry => {
    // we don't care about the response so destroy the stream if we can, or leave it flowing
    // so it can eventually finish and clean up after itself
    regFetch('/-/npm/v1/security/audits/quick', opts.concat({
      method: 'POST',
      registry,
      gzip: true,
      body: auditData
    })).then(_ => {
      _.body.on('error', () => {})
      if (_.body.destroy) {
        _.body.destroy()
      } else {
        _.body.resume()
      }
    }, _ => {})
  })
  perf.emit('time', 'audit submit')
  return regFetch('/-/npm/v1/security/audits/quick', opts.concat({
    method: 'POST',
    gzip: true,
    body: auditData
  })).then(response => {
    perf.emit('timeEnd', 'audit submit')
    perf.emit('time', 'audit body')
    return response.json()
  }).then(result => {
    perf.emit('timeEnd', 'audit body')
    return result
  })
}

function submitForFullReport (auditData) {
  perf.emit('time', 'audit submit')
  const opts = InstallAuditConfig(npmConfig())
  return regFetch('/-/npm/v1/security/audits', opts.concat({
    method: 'POST',
    gzip: true,
    body: auditData
  })).then(response => {
    perf.emit('timeEnd', 'audit submit')
    perf.emit('time', 'audit body')
    return response.json()
  }).then(result => {
    perf.emit('timeEnd', 'audit body')
    result.runId = runId
    return result
  })
}

function printInstallReport (auditResult) {
  const opts = InstallAuditConfig(npmConfig())
  return auditReport(auditResult, {
    reporter: 'install',
    withColor: opts.color,
    withUnicode: opts.unicode
  }).then(result => output(result.report))
}

function printFullReport (auditResult) {
  const opts = InstallAuditConfig(npmConfig())
  return auditReport(auditResult, {
    log: output,
    reporter: opts.json ? 'json' : 'detail',
    withColor: opts.color,
    withUnicode: opts.unicode
  }).then(result => output(result.report))
}

function printParseableReport (auditResult) {
  const opts = InstallAuditConfig(npmConfig())
  return auditReport(auditResult, {
    log: output,
    reporter: 'parseable',
    withColor: opts.color,
    withUnicode: opts.unicode
  }).then(result => output(result.report))
}

function generate (shrinkwrap, requires, diffs, install, remove) {
  const sw = cloneDeep(shrinkwrap)
  delete sw.lockfileVersion
  sw.requires = scrubRequires(requires)
  scrubDeps(sw.dependencies)

  // sw.diffs = diffs || {}
  sw.install = (install || []).map(scrubArg)
  sw.remove = (remove || []).map(scrubArg)
  return generateMetadata().then((md) => {
    sw.metadata = md
    return sw
  })
}

const scrubKeys = qw`version`
const deleteKeys = qw`from resolved`

function scrubDeps (deps) {
  if (!deps) return
  Object.keys(deps).forEach(name => {
    if (!shouldScrubName(name) && !shouldScrubSpec(name, deps[name].version)) return
    const value = deps[name]
    delete deps[name]
    deps[scrub(name)] = value
  })
  Object.keys(deps).forEach(name => {
    for (let toScrub of scrubKeys) {
      if (!deps[name][toScrub]) continue
      deps[name][toScrub] = scrubSpec(name, deps[name][toScrub])
    }
    for (let toDelete of deleteKeys) delete deps[name][toDelete]

    scrubRequires(deps[name].requires)
    scrubDeps(deps[name].dependencies)
  })
}

function scrubRequires (reqs) {
  if (!reqs) return reqs
  Object.keys(reqs).forEach(name => {
    const spec = reqs[name]
    if (shouldScrubName(name) || shouldScrubSpec(name, spec)) {
      delete reqs[name]
      reqs[scrub(name)] = scrubSpec(name, spec)
    } else {
      reqs[name] = scrubSpec(name, spec)
    }
  })
  return reqs
}

function getScope (name) {
  if (name[0] === '@') return name.slice(0, name.indexOf('/'))
}

function shouldScrubName (name) {
  const scope = getScope(name)
  const cfg = npm.config // avoid the no-dynamic-lookups test
  return Boolean(scope && cfg.get(scope + ':registry'))
}
function shouldScrubSpec (name, spec) {
  const req = npa.resolve(name, spec)
  return !req.registry
}

function scrubArg (arg) {
  const req = npa(arg)
  let name = req.name
  if (shouldScrubName(name) || shouldScrubSpec(name, req.rawSpec)) {
    name = scrubName(name)
  }
  const spec = scrubSpec(req.name, req.rawSpec)
  return name + '@' + spec
}

function scrubName (name) {
  return shouldScrubName(name) ? scrub(name) : name
}

function scrubSpec (name, spec) {
  const req = npa.resolve(name, spec)
  if (req.registry) return spec
  if (req.type === 'git') {
    return 'git+ssh://' + scrub(spec)
  } else if (req.type === 'remote') {
    return 'https://' + scrub(spec)
  } else if (req.type === 'directory') {
    return 'file:' + scrub(spec)
  } else if (req.type === 'file') {
    return 'file:' + scrub(spec) + '.tar'
  } else {
    return scrub(spec)
  }
}

module.exports.scrub = scrub
function scrub (value, rid) {
  return ssri.fromData((rid || runId) + ' ' + value, {algorithms: ['sha256']}).hexDigest()
}

function generateMetadata () {
  const meta = {}
  meta.npm_version = npm.version
  meta.node_version = process.version
  meta.platform = process.platform
  meta.node_env = process.env.NODE_ENV

  return Promise.resolve(meta)
}
/*
  const head = path.resolve(npm.prefix, '.git/HEAD')
  return readFile(head, 'utf8').then((head) => {
    if (!head.match(/^ref: /)) {
      meta.commit_hash = head.trim()
      return
    }
    const headFile = head.replace(/^ref: /, '').trim()
    meta.branch = headFile.replace(/^refs[/]heads[/]/, '')
    return readFile(path.resolve(npm.prefix, '.git', headFile), 'utf8')
  }).then((commitHash) => {
    meta.commit_hash = commitHash.trim()
    const proc = spawn('git', qw`diff --quiet --exit-code package.json package-lock.json`, {cwd: npm.prefix, stdio: 'ignore'})
    return new Promise((resolve, reject) => {
      proc.once('error', reject)
      proc.on('exit', (code, signal) => {
        if (signal == null) meta.state = code === 0 ? 'clean' : 'dirty'
        resolve()
      })
    })
  }).then(() => meta, () => meta)
*/

function generateFromInstall (tree, diffs, install, remove) {
  const requires = {}
  tree.requires.forEach((pkg) => {
    requires[pkg.package.name] = tree.package.dependencies[pkg.package.name] || tree.package.devDependencies[pkg.package.name] || pkg.package.version
  })

  const auditInstall = (install || []).filter((a) => a.name).map(packageId)
  const auditRemove = (remove || []).filter((a) => a.name).map(packageId)
  const auditDiffs = {}
  diffs.forEach((action) => {
    const mutation = action[0]
    const child = action[1]
    if (mutation !== 'add' && mutation !== 'update' && mutation !== 'remove') return
    if (!auditDiffs[mutation]) auditDiffs[mutation] = []
    if (mutation === 'add') {
      auditDiffs[mutation].push({location: child.location})
    } else if (mutation === 'update') {
      auditDiffs[mutation].push({location: child.location, previous: packageId(child.oldPkg)})
    } else if (mutation === 'remove') {
      auditDiffs[mutation].push({previous: packageId(child)})
    }
  })

  return generate(treeToShrinkwrap(tree), requires, auditDiffs, auditInstall, auditRemove)
}
