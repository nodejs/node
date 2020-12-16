const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')
const npa = require('npm-package-arg')
const semver = require('semver')

const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const output = require('./utils/output.js')

const cmd = (args, cb) => rebuild(args).then(() => cb()).catch(cb)

const usage = usageUtil('rebuild', 'npm rebuild [[<@scope>/]<name>[@<version>] ...]')

const completion = require('./utils/completion/installed-deep.js')

const rebuild = async args => {
  const globalTop = resolve(npm.globalDir, '..')
  const where = npm.flatOptions.global ? globalTop : npm.prefix
  const arb = new Arborist({
    ...npm.flatOptions,
    path: where,
  })

  if (args.length) {
    // get the set of nodes matching the name that we want rebuilt
    const tree = await arb.loadActual()
    const filter = getFilterFn(args)
    await arb.rebuild({
      nodes: tree.inventory.filter(filter),
    })
  } else
    await arb.rebuild()

  output('rebuilt dependencies successfully')
}

const getFilterFn = args => {
  const specs = args.map(arg => {
    const spec = npa(arg)
    if (spec.type === 'tag' && spec.rawSpec === '')
      return spec

    if (spec.type !== 'range' && spec.type !== 'version' && spec.type !== 'directory')
      throw new Error('`npm rebuild` only supports SemVer version/range specifiers')

    return spec
  })

  return node => specs.some(spec => {
    if (spec.type === 'directory')
      return node.path === spec.fetchSpec

    if (spec.name !== node.name)
      return false

    if (spec.rawSpec === '' || spec.rawSpec === '*')
      return true

    const { version } = node.package
    // TODO: add tests for a package with missing version
    return semver.satisfies(version, spec.fetchSpec)
  })
}

module.exports = Object.assign(cmd, { usage, completion })
