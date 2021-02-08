const Arborist = require('../')

const options = require('./lib/options.js')
const print = require('./lib/print-tree.js')
require('./lib/logging.js')
require('./lib/timers.js')

const c = require('chalk')

const whichIsA = (name, dependents, indent = '  ') => {
  if (!dependents || dependents.length === 0)
    return ''
  const str = `\nfor: ` +
    dependents.map(dep => {
      return dep.more ? `${dep.more} more (${dep.names.join(', ')})`
        : `${dep.type} dependency ` +
          `${c.bold(name)}@"${c.bold(dep.spec)}"` + `\nfrom:` +
          (dep.from.location ? (dep.from.name
            ? ` ${c.bold(dep.from.name)}@${c.bold(dep.from.version)} ` +
              c.dim(`at ${dep.from.location}`)
            : ' the root project')
          : ` ${c.bold(dep.from.name)}@${c.bold(dep.from.version)}`) +
          whichIsA(dep.from.name, dep.from.dependents, '  ')
    }).join('\nand: ')

  return str.split(/\n/).join(`\n${indent}`)
}

const explainEresolve = ({ dep, current, peerConflict, fixWithForce }) => {
  return (!dep.whileInstalling ? '' : `While resolving: ` +
  `${c.bold(dep.whileInstalling.name)}@${c.bold(dep.whileInstalling.version)}\n`) +

  `Found: ` +
    `${c.bold(current.name)}@${c.bold(current.version)} ` +
    c.dim(`at ${current.location}`) +
    `${whichIsA(current.name, current.dependents)}` +

  `\n\nCould not add conflicting dependency: ` +
    `${c.bold(dep.name)}@${c.bold(dep.version)} ` +
    c.dim(`at ${dep.location}`) +
   `${whichIsA(dep.name, dep.dependents)}\n` +

  (!peerConflict ? '' :
    `\nConflicting peer dependency: ` +
      `${c.bold(peerConflict.name)}@${c.bold(peerConflict.version)} ` +
      c.dim(`at ${peerConflict.location}`) +
      `${whichIsA(peerConflict.name, peerConflict.dependents)}\n`
  ) +

  `\nFix the upstream dependency conflict, or
run this command with  --legacy-peer-deps${
  fixWithForce ? ' or --force' : ''}
to accept an incorrect (and potentially broken) dependency resolution.
`
}

const start = process.hrtime()
new Arborist(options).buildIdealTree(options).then(tree => {
  const end = process.hrtime(start)
  print(tree)
  console.error(`resolved ${tree.inventory.size} deps in ${end[0] + end[1] / 10e9}s`)
  if (tree.meta && options.save)
    tree.meta.save()
}).catch(er => {
  console.error(er)
  if (er.code === 'ERESOLVE')
    console.error(explainEresolve(er))
})
