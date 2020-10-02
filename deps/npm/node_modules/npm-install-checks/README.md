# npm-install-checks

Check the engines and platform fields in package.json

## API

Both functions will throw an error if the check fails, or return
`undefined` if everything is ok.

Errors have a `required` and `current` fields.

### .checkEngine(pkg, npmVer, nodeVer, force = false)

Check if node/npm version is supported by the package. If it isn't
supported, an error is thrown.

`force` argument will override the node version check, but not the npm
version check, as this typically would indicate that the current version of
npm is unable to install the package properly for some reason.

Error code: 'EBADENGINE'

### .checkPlatform(pkg, force)

Check if OS/Arch is supported by the package.

Error code: 'EBADPLATFORM'
