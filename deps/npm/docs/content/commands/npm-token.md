---
title: npm-token
section: 1
description: Manage your authentication tokens
---

### Synopsis

```bash
npm token list
npm token revoke <id|token>
npm token create
```

Note: This command is unaware of workspaces.

### Description

This lets you list, create and revoke authentication tokens.

#### Listing tokens

When listing tokens, an abbreviated token will be displayed.  For security purposes the full token is not displayed.

#### Generating tokens

When generating tokens, you will be prompted you for your password and, if you have two-factor authentication enabled, an otp.

Please refer to the [docs website](https://docs.npmjs.com/creating-and-viewing-access-tokens) for more information on generating tokens for CI/CD.

#### Revoking tokens

When revoking a token, you can use the full token (e.g. what you get back from `npm token create`, or as can be found in an `.npmrc` file), or a truncated id.  If the given truncated id is not distinct enough to differentiate between multiple existing tokens, you will need to use enough of the id to allow npm to distinguish between them.  Full token ids can be found on the [npm website](https://www.npmjs.com), or in the `--parseable` or `--json` output of `npm token list`.  This command will NOT accept the truncated token found in the normal `npm token list` output.

A revoked token will immediately be removed from the registry and you will no longer be able to use it.

### Configuration

#### `name`

* Default: null
* Type: null or String

When creating a Granular Access Token with `npm token create`, this sets the
name/description for the token.



#### `token-description`

* Default: null
* Type: null or String

Description text for the token when using `npm token create`.



#### `expires`

* Default: null
* Type: null or Number

When creating a Granular Access Token with `npm token create`, this sets the
expiration in days. If not specified, the server will determine the default
expiration.



#### `packages`

* Default:
* Type: null or String (can be set multiple times)

When creating a Granular Access Token with `npm token create`, this limits
the token access to specific packages.



#### `packages-all`

* Default: false
* Type: Boolean

When creating a Granular Access Token with `npm token create`, grants the
token access to all packages instead of limiting to specific packages.



#### `scopes`

* Default: null
* Type: null or String (can be set multiple times)

When creating a Granular Access Token with `npm token create`, this limits
the token access to specific scopes. Provide a scope name (with or without @
prefix).



#### `orgs`

* Default: null
* Type: null or String (can be set multiple times)

When creating a Granular Access Token with `npm token create`, this limits
the token access to specific organizations.



#### `packages-and-scopes-permission`

* Default: null
* Type: null, "read-only", "read-write", or "no-access"

When creating a Granular Access Token with `npm token create`, sets the
permission level for packages and scopes. Options are "read-only",
"read-write", or "no-access".



#### `orgs-permission`

* Default: null
* Type: null, "read-only", "read-write", or "no-access"

When creating a Granular Access Token with `npm token create`, sets the
permission level for organizations. Options are "read-only", "read-write",
or "no-access".



#### `cidr`

* Default: null
* Type: null or String (can be set multiple times)

This is a list of CIDR address to be used when configuring limited access
tokens with the `npm token create` command.



#### `bypass-2fa`

* Default: false
* Type: Boolean

When creating a Granular Access Token with `npm token create`, setting this
to true will allow the token to bypass two-factor authentication. This is
useful for automation and CI/CD workflows.



#### `password`

* Default: null
* Type: null or String

Password for authentication. Can be provided via command line when creating
tokens, though it's generally safer to be prompted for it.



#### `registry`

* Default: "https://registry.npmjs.org/"
* Type: URL

The base URL of the npm registry.



#### `otp`

* Default: null
* Type: null or String

This is a one-time password from a two-factor authenticator. It's needed
when publishing or changing package permissions with `npm access`.

If not set, and a registry response fails with a challenge for a one-time
password, npm will prompt on the command line for one.



#### `read-only`

* Default: false
* Type: Boolean

This is used to mark a token as unable to publish when configuring limited
access tokens with the `npm token create` command.



### See Also

* [npm adduser](/commands/npm-adduser)
* [npm registry](/using-npm/registry)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [npm owner](/commands/npm-owner)
* [npm whoami](/commands/npm-whoami)
* [npm profile](/commands/npm-profile)
