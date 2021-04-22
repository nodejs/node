const { resolve } = require('path')
const Arborist = require('@npmcli/arborist')

const installedDeep = async (npm) => {
  const {
    depth,
    global,
    prefix,
  } = npm.flatOptions

  const getValues = (tree) =>
    [...tree.inventory.values()]
      .filter(i => i.location !== '' && !i.isRoot)
      .map(i => {
        return i
      })
      .filter(i => (i.depth - 1) <= depth)
      .sort((a, b) => a.depth - b.depth)
      .sort((a, b) => a.depth === b.depth ? a.name.localeCompare(b.name) : 0)

  const res = new Set()
  const gArb = new Arborist({ global: true, path: resolve(npm.globalDir, '..') })
  const gTree = await gArb.loadActual({ global: true })

  for (const node of getValues(gTree))
    res.add(global ? node.name : [node.name, '-g'])

  if (!global) {
    const arb = new Arborist({ global: false, path: prefix })
    const tree = await arb.loadActual()
    for (const node of getValues(tree))
      res.add(node.name)
  }

  return [...res]
}

module.exports = installedDeep
