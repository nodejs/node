const Arborist = require('@npmcli/arborist')
const { resolve } = require('path')
const ignore = resolve(__dirname, '../node_modules/.gitignore')
const { writeFileSync } = require('fs')
const pj = resolve(__dirname, '../package.json')
const pkg = require(pj)
const bundle = []
const arb = new Arborist({ path: resolve(__dirname, '..') })
const shouldIgnore = []

arb.loadVirtual().then(tree => {
  for (const [name, node] of tree.children.entries()) {
    if (node.dev) {
      console.error('ignore', node.name)
      shouldIgnore.push(node.name)
    } else if (tree.edgesOut.has(node.name)) {
      console.error('BUNDLE', node.name)
      bundle.push(node.name)
    }
  }
  pkg.bundleDependencies = bundle.sort((a, b) => a.localeCompare(b))

  const ignores = shouldIgnore.sort((a, b) => a.localeCompare(b))
    .map(i => `/${i}`)
    .join('\n')
  const ignoreData = `# Automatically generated to ignore dev deps
/.package-lock.json
package-lock.json
${ignores}
`
  writeFileSync(ignore, ignoreData)
  writeFileSync(pj, JSON.stringify(pkg, 0, 2) + '\n')
})
