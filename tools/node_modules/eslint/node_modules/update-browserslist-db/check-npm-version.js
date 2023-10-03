let { execSync } = require('child_process')
let pico = require('picocolors')

try {
  let version = parseInt(execSync('npm -v'))
  if (version <= 6) {
    process.stderr.write(
      pico.red(
        'Update npm or call ' +
          pico.yellow('npx update-browserslist-db@latest') +
          '\n'
      )
    )
    process.exit(1)
  }
} catch (e) {}
