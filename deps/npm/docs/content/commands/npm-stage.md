---
title: npm-stage
section: 1
description: Stage packages for publishing
---

### Synopsis

```bash
npm stage
```

Note: This command is unaware of workspaces.

### Description

Staged publishing allows package maintainers to require proof-of-presence
for all publishes. Proof-of-presence is where a human is involved,
interjects, and provides authentication (2FA) during an action — in this
case, publishing an npm package.

Typically when maintainers use automated workflows to publish,
proof-of-presence is lacking as there's no convenient way to interject the
process and provide 2FA, as is the case for publishing with a granular
access token with bypass and the trusted publishing flow. Staged publishing
allows users to have their automated workflows stage a package without a 2FA
prompt, deferring the act of 2FA, allowing the maintainer to approve the
staged package and publish at a later point.

The `npm stage publish` command packs the current working directory and
places that version of the package into the registry in a state where it's
not available for public access, allowing maintainers to approve the package
at a later point in time. The act of staging does not prompt for 2FA and can be done with any token
type, the act of approving will.

Key behaviors:

* Staged packages share the same semver version unique index as published
  packages — you cannot publish a version that already exists as a staged
  version for that package.
* You can still publish packages normally while you have staged packages
  pending.
* You can stage multiple versions of the same package.
* `npm stage publish` has parity with `npm publish` and will respect
  `"private": true` in `package.json`, refusing to stage the package.

### Prerequisites

Before using `npm stage` commands, ensure the following requirements are met:

* **Write permissions on the package:** You must have write access to the
  package you're configuring.
* **Package must exist:** The package you're configuring must already exist
  on the npm registry.
* **2FA enabled on your account:** Commands that require 2FA will prompt you
  to authenticate. If you don't already have 2FA enabled on your account,
  you must enable it before using these commands.

### Subcommands

* `npm stage publish [<package-spec>]` - Stage a package for publishing
* `npm stage list [<package-spec>]` - List all staged package versions
* `npm stage view <stage-id>` - View details of a specific staged package
* `npm stage approve <stage-id>` - Approve a staged package for publishing
* `npm stage reject <stage-id>` - Reject a staged package
* `npm stage download <stage-id>` - Download the tarball for inspection

### 2FA Requirements by Subcommand

| Command | Requires 2FA | Notes |
| --- | --- | --- |
| `npm stage publish` | No | Designed for automated workflows; defers 2FA to approval |
| `npm stage list` | No | View staged packages |
| `npm stage view` | No | View staged package details |
| `npm stage approve` | Yes | Prompts for 2FA to publish the staged package |
| `npm stage reject` | Yes | Prompts for 2FA to permanently remove the staged package |
| `npm stage download` | No | Downloads the tarball for local inspection |

### Tag Behavior

The `--tag` flag follows the same logic as `npm publish`. If no tag is
provided, the `latest` tag is used by default. For pre-release versions
(e.g., `1.0.0-beta.1`) and non-latest semver versions, the tag must be
explicitly provided — otherwise the CLI will error, just as `npm publish`
would.

The tag is an immutable property of the staged package. Once a package is
staged with a given tag, the tag cannot be changed. If you need to stage the
same version with a different tag, you must first reject the existing staged
package using `npm stage reject` and then re-stage it with the desired tag.

### Token Behavior

The key difference with staged publishing is that `npm stage publish` never
requires a 2FA prompt, regardless of token type. This is what makes it
suitable for automated workflows. The goal of `npm stage publish` is
deferring proof-of-presence to a later point in time.

| Token Type | `npm stage publish` | `npm publish` |
| --- | --- | --- |
| GAT with bypass | Can stage | Can publish (if allowed by package publishing access) |
| GAT without bypass | Can stage | 2FA prompt (if allowed by package publishing access) |
| Session token | Can stage | 2FA prompt |
| Trust token (OIDC) | Can stage (if allowed) | Can publish (if allowed) |

### Trust Relationship Permissions

With staged publishing, trust relationships now support granular command
permissions. Shortlived tokens issued through trust relationships can only be
used with `npm stage publish` and `npm publish`. Shortlived tokens cannot run
`npm stage` subcommands.

`npm trust <provider>` supports `--allow-publish` and `--allow-stage-publish`
to control which commands are available through each trust relationship.

### Best Practices

**Note:** The addition of staged publishing does not make your account or org
more secure. Maintainers must still use the best practices listed below.

1. **Delete Granular Access Tokens (GAT) with bypass 2FA enabled.**
   Now with staged publishing, we've eliminated the need for a GAT token
   that can bypass 2FA. We encourage you to delete all your tokens with
   bypass enabled and switch to using a trust relationship in your automated
   workflows, or create a GAT without bypass and use `npm stage publish`.

2. **Disallow tokens from publishing at the package level.**
   All packages have their own access controls under "package access"
   allowing packages to be published with bypass tokens, which is no longer
   a necessity. We encourage you to select "Require two-factor
   authentication and disallow tokens (recommended)" for all your packages
   on the package access page.

3. **Configure trust relationship permissions to prevent `npm publish`.**
   We encourage you to only enable `npm stage publish` on your trust
   relationships and disable `npm publish`.

### Configuration

### `npm stage publish`

Stage a package for publishing, deferring proof-of-presence (2FA) to a later point in time

#### Synopsis

```bash
npm stage publish <package-spec>
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--tag` | "latest" | String | If you ask npm to install a package and don't tell it a specific version,       then it will install the specified tag.        It is the tag added to the package@version specified in the        `npm dist-tag add` command, if no explicit tag is given.        When used by the `npm diff` command, this is the tag used to fetch the       tarball that will be compared with the local files by default.              If used in the `npm publish` command, this is the tag that will be        added to the package submitted to the registry. |
| `--access` | 'public' for new packages, existing packages it will not change the current level | null, "restricted", "public", or "private" | If you do not want your scoped package to be publicly viewable (and     installable) set `--access=restricted`.      Unscoped packages cannot be set to `restricted`.      Note: This defaults to not changing the current access level for existing     packages.  Specifying a value of `restricted` or `public` during     publish will change the access for an existing package the same way that     `npm access set status` would.      The value `private` is an alias for `restricted`. |
| `--dry-run` | false | Boolean | Indicates that you don't want npm to make any changes and that it should       only report what it would have done.  This can be passed into any of the       commands that modify your local installation, eg, `install`,       `update`, `dedupe`, `uninstall`, as well as `pack` and       `publish`.        Note: This is NOT honored by other network related commands, eg       `dist-tags`, `owner`, etc. |
| `--otp` | null | null or String | This is a one-time password from a two-factor authenticator.  It's needed       when publishing or changing package permissions with `npm access`.        If not set, and a registry response fails with a challenge for a one-time       password, npm will prompt on the command line for one. |
| `--workspace`, `-w` |  | String (can be set multiple times) | Enable running a command in the context of the configured workspaces of the       current project while filtering by running only the workspaces defined by       this configuration option.        Valid values for the `workspace` config are either:        * Workspace names       * Path to a workspace directory       * Path to a parent workspace directory (will result in selecting all         workspaces within that folder)        When set for the `npm init` command, this may be set to the folder of       a workspace which does not yet exist, to create the folder and set it       up as a brand new workspace within the project. |
| `--workspaces` | null | null or Boolean | Set to true to run the command in the context of **all** configured       workspaces.        Explicitly setting this to false will cause commands like `install` to       ignore workspaces altogether.       When not set explicitly:        - Commands that operate on the `node_modules` tree (install, update,         etc.) will link workspaces into the `node_modules` folder.       - Commands that do other things (test, exec, publish, etc.) will operate         on the root project, _unless_ one or more workspaces are specified in         the `workspace` config. |
| `--include-workspace-root` | false | Boolean | Include the workspace root when workspaces are enabled for a command.        When false, specifying individual workspaces via the `workspace` config,       or all workspaces via the `workspaces` flag, will cause npm to operate only       on the specified workspaces, and not on the root project. |
| `--provenance` | false | Boolean | When publishing from a supported cloud CI/CD system, the package will be       publicly linked to where it was built and published from. |

### `npm stage list`

List all staged package versions

#### Synopsis

```bash
npm stage list [<package-spec>]
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--json` | false | Boolean | Whether or not to output JSON data, rather than the normal output.        * In `npm pkg set` it enables parsing set values with JSON.parse()       before saving them to your `package.json`.        Not supported by all npm commands. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |

### `npm stage view`

View details of a specific staged package

#### Synopsis

```bash
npm stage view <stage-id>
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--json` | false | Boolean | Whether or not to output JSON data, rather than the normal output.        * In `npm pkg set` it enables parsing set values with JSON.parse()       before saving them to your `package.json`.        Not supported by all npm commands. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |

### `npm stage approve`

Approve a staged package, publishing it to the npm registry

#### Synopsis

```bash
npm stage approve <stage-id>
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--otp` | null | null or String | This is a one-time password from a two-factor authenticator.  It's needed       when publishing or changing package permissions with `npm access`.        If not set, and a registry response fails with a challenge for a one-time       password, npm will prompt on the command line for one. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |

### `npm stage reject`

Reject a staged package, removing it from the registry

#### Synopsis

```bash
npm stage reject <stage-id>
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--otp` | null | null or String | This is a one-time password from a two-factor authenticator.  It's needed       when publishing or changing package permissions with `npm access`.        If not set, and a registry response fails with a challenge for a one-time       password, npm will prompt on the command line for one. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |

### `npm stage download`

Download the tarball of a staged package for inspection

#### Synopsis

```bash
npm stage download <stage-id>
```

#### Flags

| Flag | Default | Type | Description |
| --- | --- | --- | --- |
| `--json` | false | Boolean | Whether or not to output JSON data, rather than the normal output.        * In `npm pkg set` it enables parsing set values with JSON.parse()       before saving them to your `package.json`.        Not supported by all npm commands. |
| `--registry` | "https://registry.npmjs.org/" | URL | The base URL of the npm registry. |


### See Also

* [npm publish](/commands/npm-publish)
* [npm unpublish](/commands/npm-unpublish)
* [npm trust](/commands/npm-trust)
