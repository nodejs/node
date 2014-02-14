# npm-install-checks

A package that contains checks that npm runs during the installation.

## API

### .checkEngine(target, npmVer, nodeVer, force, strict, cb)
Check if node/npm version is supported by the package.

Error type: `ENOTSUP`

### .checkPlatform(target, force, cb)
Check if OS/Arch is supported by the package.

Error type: `EBADPLATFORM`

### .checkCycle(target, ancestors, cb)
Check for cyclic dependencies.

Error type: `ECYCLE`

### .checkGit(folder, cb)
Check if a folder is a .git folder.

Error type: `EISGIT`
