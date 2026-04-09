const reifyOutput = require('./reify-output.js')
const ini = require('ini')
const { writeFile } = require('node:fs/promises')
const { resolve } = require('node:path')

const reifyFinish = async (npm, arb) => {
  // if we are using a builtin config, and just installed npm as a top-level global package, we have to preserve that config.
  if (arb.options.global) {
    const npmNode = arb.actualTree.inventory.get('node_modules/npm')
    if (npmNode) {
      const builtinConf = npm.config.data.get('builtin')
      if (!builtinConf.loadError) {
        const content = ini.stringify(builtinConf.raw).trim() + '\n'
        await writeFile(resolve(npmNode.path, 'npmrc'), content)
      }
    }
  }
  reifyOutput(npm, arb)
}

module.exports = reifyFinish
