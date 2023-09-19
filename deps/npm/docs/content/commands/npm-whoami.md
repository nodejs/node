---
title: npm-whoami
section: 1
description: Display npm username
---

### Synopsis

```bash
npm whoami
```

Note: This command is unaware of workspaces.

### Description

Display the npm username of the currently logged-in user.

If logged into a registry that provides token-based authentication, then
connect to the `/-/whoami` registry endpoint to find the username
associated with the token, and print to standard output.

If logged into a registry that uses Basic Auth, then simply print the
`username` portion of the authentication string.

### Configuration

#### `registry`

* Default: "https://registry.npmjs.org/"
* Type: URL

The base URL of the npm registry.



### See Also

* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [npm adduser](/commands/npm-adduser)
