const { bundleDependencies } = require('./package.json')
const { spawnSync } = require('child_process')
for (const dep of bundleDependencies) {
  const lib = spawnSync('git', ['grep', dep, 'lib'])
  const bin = spawnSync('git', ['grep', dep, 'bin'])
  if (!lib.stdout.length && !bin.stdout.length) {
    console.log(dep)
  }
}
