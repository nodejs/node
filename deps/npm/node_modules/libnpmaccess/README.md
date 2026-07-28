# libnpmaccess

[![npm version](https://img.shields.io/npm/v/libnpmaccess.svg)](https://npm.im/libnpmaccess)
[![license](https://img.shields.io/npm/l/libnpmaccess.svg)](https://npm.im/libnpmaccess)
[![CI - libnpmaccess](https://github.com/npm/cli/actions/workflows/ci-libnpmaccess.yml/badge.svg)](https://github.com/npm/cli/actions/workflows/ci-libnpmaccess.yml)

[`libnpmaccess`](https://github.com/npm/libnpmaccess) is a Node.js
library that provides programmatic access to the guts of the npm CLI's `npm
access` command. This includes managing account mfa settings, listing
packages and permissions, looking at package collaborators, and defining
package permissions for users, orgs, and teams.

## Example

```javascript
const access = require('libnpmaccess')
const opts = { '//registry.npmjs.org/:_authToken: 'npm_token }

// List all packages @zkat has access to on the npm registry.
console.log(Object.keys(await access.getPackages('zkat', opts)))
```

### API

#### `opts` for all `libnpmaccess` commands

`libnpmaccess` uses [`npm-registry-fetch`](https://npm.im/npm-registry-fetch).

All options are passed through directly to that library, so please refer
to [its own `opts`
documentation](https://www.npmjs.com/package/npm-registry-fetch#fetch-options)
for options that can be passed in.

#### `spec` parameter for all `libnpmaccess` commands

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec.

#### `access.getCollaborators(spec, opts) -> Promise<Object>`

Gets collaborators for a given package

#### `access.getPackages(user|scope|team, opts) -> Promise<Object>`

Gets all packages for a given user, scope, or team.

Teams should be in the format `scope:team` or `@scope:team`

Users and scopes can be in the format `@scope` or `scope`

#### `access.getVisibility(spec, opts) -> Promise<Object>`

Gets the visibility of a given package

#### `access.removePermissions(team, spec, opts) -> Promise<Boolean>`

Removes the access for a given team to a package.

Teams should be in the format `scope:team` or `@scope:team`

#### `access.setAccess(package, access, opts) -> Promise<Boolean>`

Sets access level for package described by `spec`.

The npm registry accepts the following `access` levels:

- `public`: package is public
- `private`: package is private

The npm registry also only allows scoped packages to have their access
level set.

#### access.setMfa(spec, level, opts) -> Promise<Boolean>`

Sets the publishing mfa requirements for a given package.  Level must be one of the
following:

- `none`: mfa is not required to publish this package.
- `publish`: mfa is required to publish this package, automation tokens
cannot be used to publish.
- `automation`: mfa is required to publish this package, automation tokens
may also be used for publishing from continuous integration workflows.

#### access.setPermissions(team, spec, permissions, opts) -> Promise<Boolean>`

Sets permissions levels for a given team to a package.

Teams should be in the format `scope:team` or `@scope:team`

The npm registry accepts the following `permissions`:

- `read-only`: Read only permissions
- `read-write`: Read and write (aka publish) permissions
