const Arborist = require('@npmcli/arborist')
const auditReport = require('npm-audit-report')
const fetch = require('npm-registry-fetch')
const localeCompare = require('@isaacs/string-locale-compare')('en')
const npa = require('npm-package-arg')
const pacote = require('pacote')
const pMap = require('p-map')

const ArboristWorkspaceCmd = require('../arborist-cmd.js')
const auditError = require('../utils/audit-error.js')
const log = require('../utils/log-shim.js')
const reifyFinish = require('../utils/reify-finish.js')

const sortAlphabetically = (a, b) => localeCompare(a.name, b.name)

class VerifySignatures {
  constructor (tree, filterSet, npm, opts) {
    this.tree = tree
    this.filterSet = filterSet
    this.npm = npm
    this.opts = opts
    this.keys = new Map()
    this.invalid = []
    this.missing = []
    this.checkedPackages = new Set()
    this.auditedWithKeysCount = 0
    this.verifiedSignatureCount = 0
    this.verifiedAttestationCount = 0
    this.exitCode = 0
  }

  async run () {
    const start = process.hrtime.bigint()

    // Find all deps in tree
    const { edges, registries } = this.getEdgesOut(this.tree.inventory.values(), this.filterSet)
    if (edges.size === 0) {
      throw new Error('found no installed dependencies to audit')
    }

    await Promise.all([...registries].map(registry => this.setKeys({ registry })))

    const progress = log.newItem('verifying registry signatures', edges.size)
    const mapper = async (edge) => {
      progress.completeWork(1)
      await this.getVerifiedInfo(edge)
    }
    await pMap(edges, mapper, { concurrency: 20, stopOnError: true })

    // Didn't find any dependencies that could be verified, e.g. only local
    // deps, missing version, not on a registry etc.
    if (!this.auditedWithKeysCount) {
      throw new Error('found no dependencies to audit that where installed from ' +
                      'a supported registry')
    }

    const invalid = this.invalid.sort(sortAlphabetically)
    const missing = this.missing.sort(sortAlphabetically)

    const hasNoInvalidOrMissing = invalid.length === 0 && missing.length === 0

    if (!hasNoInvalidOrMissing) {
      process.exitCode = 1
    }

    if (this.npm.config.get('json')) {
      this.npm.output(JSON.stringify({
        invalid,
        missing,
      }, null, 2))
      return
    }
    const end = process.hrtime.bigint()
    const elapsed = end - start

    const auditedPlural = this.auditedWithKeysCount > 1 ? 's' : ''
    const timing = `audited ${this.auditedWithKeysCount} package${auditedPlural} in ` +
      `${Math.floor(Number(elapsed) / 1e9)}s`
    this.npm.output(timing)
    this.npm.output('')

    const verifiedBold = this.npm.chalk.bold('verified')
    if (this.verifiedSignatureCount) {
      if (this.verifiedSignatureCount === 1) {
        /* eslint-disable-next-line max-len */
        this.npm.output(`${this.verifiedSignatureCount} package has a ${verifiedBold} registry signature`)
      } else {
        /* eslint-disable-next-line max-len */
        this.npm.output(`${this.verifiedSignatureCount} packages have ${verifiedBold} registry signatures`)
      }
      this.npm.output('')
    }

    if (this.verifiedAttestationCount) {
      if (this.verifiedAttestationCount === 1) {
        /* eslint-disable-next-line max-len */
        this.npm.output(`${this.verifiedAttestationCount} package has a ${verifiedBold} attestation`)
      } else {
        /* eslint-disable-next-line max-len */
        this.npm.output(`${this.verifiedAttestationCount} packages have ${verifiedBold} attestations`)
      }
      this.npm.output('')
    }

    if (missing.length) {
      const missingClr = this.npm.chalk.bold(this.npm.chalk.red('missing'))
      if (missing.length === 1) {
        /* eslint-disable-next-line max-len */
        this.npm.output(`1 package has a ${missingClr} registry signature but the registry is providing signing keys:`)
      } else {
        /* eslint-disable-next-line max-len */
        this.npm.output(`${missing.length} packages have ${missingClr} registry signatures but the registry is providing signing keys:`)
      }
      this.npm.output('')
      missing.map(m =>
        this.npm.output(`${this.npm.chalk.red(`${m.name}@${m.version}`)} (${m.registry})`)
      )
    }

    if (invalid.length) {
      if (missing.length) {
        this.npm.output('')
      }
      const invalidClr = this.npm.chalk.bold(this.npm.chalk.red('invalid'))
      // We can have either invalid signatures or invalid provenance
      const invalidSignatures = this.invalid.filter(i => i.code === 'EINTEGRITYSIGNATURE')
      if (invalidSignatures.length) {
        if (invalidSignatures.length === 1) {
          this.npm.output(`1 package has an ${invalidClr} registry signature:`)
        } else {
          /* eslint-disable-next-line max-len */
          this.npm.output(`${invalidSignatures.length} packages have ${invalidClr} registry signatures:`)
        }
        this.npm.output('')
        invalidSignatures.map(i =>
          this.npm.output(`${this.npm.chalk.red(`${i.name}@${i.version}`)} (${i.registry})`)
        )
        this.npm.output('')
      }

      const invalidAttestations = this.invalid.filter(i => i.code === 'EATTESTATIONVERIFY')
      if (invalidAttestations.length) {
        if (invalidAttestations.length === 1) {
          this.npm.output(`1 package has an ${invalidClr} attestation:`)
        } else {
          /* eslint-disable-next-line max-len */
          this.npm.output(`${invalidAttestations.length} packages have ${invalidClr} attestations:`)
        }
        this.npm.output('')
        invalidAttestations.map(i =>
          this.npm.output(`${this.npm.chalk.red(`${i.name}@${i.version}`)} (${i.registry})`)
        )
        this.npm.output('')
      }

      if (invalid.length === 1) {
        /* eslint-disable-next-line max-len */
        this.npm.output(`Someone might have tampered with this package since it was published on the registry!`)
      } else {
        /* eslint-disable-next-line max-len */
        this.npm.output(`Someone might have tampered with these packages since they were published on the registry!`)
      }
      this.npm.output('')
    }
  }

  getEdgesOut (nodes, filterSet) {
    const edges = new Set()
    const registries = new Set()
    for (const node of nodes) {
      for (const edge of node.edgesOut.values()) {
        const filteredOut =
          edge.from
            && filterSet
            && filterSet.size > 0
            && !filterSet.has(edge.from.target)

        if (!filteredOut) {
          const spec = this.getEdgeSpec(edge)
          if (spec) {
            // Prefetch and cache public keys from used registries
            registries.add(this.getSpecRegistry(spec))
          }
          edges.add(edge)
        }
      }
    }
    return { edges, registries }
  }

  async setKeys ({ registry }) {
    const keys = await fetch.json('/-/npm/v1/keys', {
      ...this.npm.flatOptions,
      registry,
    }).then(({ keys: ks }) => ks.map((key) => ({
      ...key,
      pemkey: `-----BEGIN PUBLIC KEY-----\n${key.key}\n-----END PUBLIC KEY-----`,
    }))).catch(err => {
      if (err.code === 'E404' || err.code === 'E400') {
        return null
      } else {
        throw err
      }
    })
    if (keys) {
      this.keys.set(registry, keys)
    }
  }

  getEdgeType (edge) {
    return edge.optional ? 'optionalDependencies'
      : edge.peer ? 'peerDependencies'
      : edge.dev ? 'devDependencies'
      : 'dependencies'
  }

  getEdgeSpec (edge) {
    let name = edge.name
    try {
      name = npa(edge.spec).subSpec.name
    } catch {
      // leave it as edge.name
    }
    try {
      return npa(`${name}@${edge.spec}`)
    } catch {
      // Skip packages with invalid spec
    }
  }

  buildRegistryConfig (registry) {
    const keys = this.keys.get(registry) || []
    const parsedRegistry = new URL(registry)
    const regKey = `//${parsedRegistry.host}${parsedRegistry.pathname}`
    return {
      [`${regKey}:_keys`]: keys,
    }
  }

  getSpecRegistry (spec) {
    return fetch.pickRegistry(spec, this.npm.flatOptions)
  }

  getValidPackageInfo (edge) {
    const type = this.getEdgeType(edge)
    // Skip potentially optional packages that are not on disk, as these could
    // be omitted during install
    if (edge.error === 'MISSING' && type !== 'dependencies') {
      return
    }

    const spec = this.getEdgeSpec(edge)
    // Skip invalid version requirements
    if (!spec) {
      return
    }
    const node = edge.to || edge
    const { version } = node.package || {}

    if (node.isWorkspace || // Skip local workspaces packages
        !version || // Skip packages that don't have a installed version, e.g. optonal dependencies
        !spec.registry) { // Skip if not from registry, e.g. git package
      return
    }

    for (const omitType of this.npm.config.get('omit')) {
      if (node[omitType]) {
        return
      }
    }

    return {
      name: spec.name,
      version,
      type,
      location: node.location,
      registry: this.getSpecRegistry(spec),
    }
  }

  async verifySignatures (name, version, registry) {
    const {
      _integrity: integrity,
      _signatures,
      _attestations,
      _resolved: resolved,
    } = await pacote.manifest(`${name}@${version}`, {
      verifySignatures: true,
      verifyAttestations: true,
      ...this.buildRegistryConfig(registry),
      ...this.npm.flatOptions,
    })
    const signatures = _signatures || []
    const result = {
      integrity,
      signatures,
      attestations: _attestations,
      resolved,
    }
    return result
  }

  async getVerifiedInfo (edge) {
    const info = this.getValidPackageInfo(edge)
    if (!info) {
      return
    }
    const { name, version, location, registry, type } = info
    if (this.checkedPackages.has(location)) {
      // we already did or are doing this one
      return
    }
    this.checkedPackages.add(location)

    // We only "audit" or verify the signature, or the presence of it, on
    // packages whose registry returns signing keys
    const keys = this.keys.get(registry) || []
    if (keys.length) {
      this.auditedWithKeysCount += 1
    }

    try {
      const { integrity, signatures, attestations, resolved } = await this.verifySignatures(
        name, version, registry
      )

      // Currently we only care about missing signatures on registries that provide a public key
      // We could make this configurable in the future with a strict/paranoid mode
      if (signatures.length) {
        this.verifiedSignatureCount += 1
      } else if (keys.length) {
        this.missing.push({
          integrity,
          location,
          name,
          registry,
          resolved,
          version,
        })
      }

      // Track verified attestations separately to registry signatures, as all
      // packages on registries with signing keys are expected to have registry
      // signatures, but not all packages have provenance and publish attestations.
      if (attestations) {
        this.verifiedAttestationCount += 1
      }
    } catch (e) {
      if (e.code === 'EINTEGRITYSIGNATURE' || e.code === 'EATTESTATIONVERIFY') {
        this.invalid.push({
          code: e.code,
          message: e.message,
          integrity: e.integrity,
          keyid: e.keyid,
          location,
          name,
          registry,
          resolved: e.resolved,
          signature: e.signature,
          predicateType: e.predicateType,
          type,
          version,
        })
      } else {
        throw e
      }
    }
  }
}

class Audit extends ArboristWorkspaceCmd {
  static description = 'Run a security audit'
  static name = 'audit'
  static params = [
    'audit-level',
    'dry-run',
    'force',
    'json',
    'package-lock-only',
    'omit',
    'foreground-scripts',
    'ignore-scripts',
    ...super.params,
  ]

  static usage = ['[fix|signatures]']

  async completion (opts) {
    const argv = opts.conf.argv.remain

    if (argv.length === 2) {
      return ['fix', 'signatures']
    }

    switch (argv[2]) {
      case 'fix':
      case 'signatures':
        return []
      default:
        throw Object.assign(new Error(argv[2] + ' not recognized'), {
          code: 'EUSAGE',
        })
    }
  }

  async exec (args) {
    if (args[0] === 'signatures') {
      await this.auditSignatures()
    } else {
      await this.auditAdvisories(args)
    }
  }

  async auditAdvisories (args) {
    const reporter = this.npm.config.get('json') ? 'json' : 'detail'
    const opts = {
      ...this.npm.flatOptions,
      audit: true,
      path: this.npm.prefix,
      reporter,
      workspaces: this.workspaceNames,
    }

    const arb = new Arborist(opts)
    const fix = args[0] === 'fix'
    await arb.audit({ fix })
    if (fix) {
      await reifyFinish(this.npm, arb)
    } else {
      // will throw if there's an error, because this is an audit command
      auditError(this.npm, arb.auditReport)
      const result = auditReport(arb.auditReport, opts)
      process.exitCode = process.exitCode || result.exitCode
      this.npm.output(result.report)
    }
  }

  async auditSignatures () {
    if (this.npm.global) {
      throw Object.assign(
        new Error('`npm audit signatures` does not support global packages'), {
          code: 'EAUDITGLOBAL',
        }
      )
    }

    log.verbose('loading installed dependencies')
    const opts = {
      ...this.npm.flatOptions,
      path: this.npm.prefix,
      workspaces: this.workspaceNames,
    }

    const arb = new Arborist(opts)
    const tree = await arb.loadActual()
    let filterSet = new Set()
    if (opts.workspaces && opts.workspaces.length) {
      filterSet =
        arb.workspaceDependencySet(
          tree,
          opts.workspaces,
          this.npm.flatOptions.includeWorkspaceRoot
        )
    } else if (!this.npm.flatOptions.workspacesEnabled) {
      filterSet =
        arb.excludeWorkspacesDependencySet(tree)
    }

    const verify = new VerifySignatures(tree, filterSet, this.npm, { ...opts })
    await verify.run()
  }
}

module.exports = Audit
