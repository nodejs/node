#!/usr/bin/env node
const [cmd] = process.argv.splice(2, 1)

const usage = () => `Arborist - the npm tree doctor

Version: ${require('../package.json').version}

# USAGE
  arborist <cmd> [path] [options...]

# COMMANDS

* reify: reify ideal tree to node_modules (install, update, rm, ...)
* ideal: generate and print the ideal tree
* actual: read and print the actual tree in node_modules
* virtual: read and print the virtual tree in the local shrinkwrap file
* shrinkwrap: load a local shrinkwrap and print its data
* audit: perform a security audit on project dependencies
* funding: query funding information in the local package tree.  A second
  positional argument after the path name can limit to a package name.
* license: query license information in the local package tree.  A second
  positional argument after the path name can limit to a license type.
* help: print this text

# OPTIONS

Most npm options are supported, but in camelCase rather than css-case.  For
example, instead of '--dry-run', use '--dryRun'.

Additionally:

* --quiet will supppress the printing of package trees
* Instead of 'npm install <pkg>', use 'arborist reify --add=<pkg>'.
  The '--add=<pkg>' option can be specified multiple times.
* Instead of 'npm rm <pkg>', use 'arborist reify --rm=<pkg>'.
  The '--rm=<pkg>' option can be specified multiple times.
* Instead of 'npm update', use 'arborist reify --update-all'.
* 'npm audit fix' is 'arborist audit --fix'
`

const help = () => console.log(usage())

switch (cmd) {
  case 'actual':
    require('./actual.js')
    break
  case 'virtual':
    require('./virtual.js')
    break
  case 'ideal':
    require('./ideal.js')
    break
  case 'reify':
    require('./reify.js')
    break
  case 'audit':
    require('./audit.js')
    break
  case 'funding':
    require('./funding.js')
    break
  case 'license':
    require('./license.js')
    break
  case 'shrinkwrap':
    require('./shrinkwrap.js')
    break
  case 'help':
  case '-h':
  case '--help':
    help()
    break
  default:
    process.exitCode = 1
    console.error(usage())
    break
}
