---
title: npm-logout
section: 1
description: Log out of the registry
---

### Synopsis

```bash
npm logout
```

Note: This command is unaware of workspaces.

### Description

When logged into a registry that supports token-based authentication, tell
the server to end this token's session. This will invalidate the token
everywhere you're using it, not just for the current environment.

When logged into a legacy registry that uses username and password
authentication, this will clear the credentials in your user configuration.
In this case, it will _only_ affect the current environment.

If `--scope` is provided, this will find the credentials for the registry
connected to that scope, if set.

### Configuration

#### `registry`

* Default: "https://registry.npmjs.org/"
* Type: URL

The base URL of the npm registry.



#### `scope`

* Default: the scope of the current project, if any, or ""
* Type: String

Associate an operation with a scope for a scoped registry.

Useful when logging in to or out of a private registry:

```
# log in, linking the scope to the custom registry
npm login --scope=@mycorp --registry=https://registry.mycorp.com

# log out, removing the link and the auth token
npm logout --scope=@mycorp
```

This will cause `@mycorp` to be mapped to the registry for future
installation of packages specified according to the pattern
`@mycorp/package`.

This will also cause `npm init` to create a scoped package.

```
# accept all defaults, and create a package named "@foo/whatever",
# instead of just named "whatever"
npm init --scope=@foo --yes
```



### See Also

* [npm adduser](/commands/npm-adduser)
* [npm registry](/using-npm/registry)
* [npm config](/commands/npm-config)
* [npm whoami](/commands/npm-whoami)
