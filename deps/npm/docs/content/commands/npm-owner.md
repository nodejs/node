---
title: npm-owner
section: 1
description: Manage package owners
---

### Synopsis

```bash
npm owner add <user> <package-spec>
npm owner rm <user> <package-spec>
npm owner ls <package-spec>

alias: author
```

### Description

Manage ownership of published packages.

* ls: List all the users who have access to modify a package and push new
  versions.  Handy when you need to know who to bug for help.
* add: Add a new user as a maintainer of a package.  This user is enabled
  to modify metadata, publish new versions, and add other owners.
* rm: Remove a user from the package owner list.  This immediately revokes
  their privileges.

Note that there is only one level of access.  Either you can modify a package,
or you can't.  Future versions may contain more fine-grained access levels, but
that is not implemented at this time.

If you have two-factor authentication enabled with `auth-and-writes` (see
[`npm-profile`](/commands/npm-profile)) then you'll need to include an otp
on the command line when changing ownership with `--otp`.

### Configuration

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

#### `workspace`

* Default:
* Type: String (can be set multiple times)

Enable running a command in the context of the configured workspaces of the
current project while filtering by running only the workspaces defined by
this configuration option.

Valid values for the `workspace` config are either:

* Workspace names
* Path to a workspace directory
* Path to a parent workspace directory (will result in selecting all
  workspaces within that folder)

When set for the `npm init` command, this may be set to the folder of a
workspace which does not yet exist, to create the folder and set it up as a
brand new workspace within the project.

This value is not exported to the environment for child processes.

#### `workspaces`

* Default: null
* Type: null or Boolean

Set to true to run the command in the context of **all** configured
workspaces.

Explicitly setting this to false will cause commands like `install` to
ignore workspaces altogether. When not set explicitly:

- Commands that operate on the `node_modules` tree (install, update, etc.)
will link workspaces into the `node_modules` folder. - Commands that do
other things (test, exec, publish, etc.) will operate on the root project,
_unless_ one or more workspaces are specified in the `workspace` config.

This value is not exported to the environment for child processes.

### See Also

* [package spec](/using-npm/package-spec)
* [npm profile](/commands/npm-profile)
* [npm publish](/commands/npm-publish)
* [npm registry](/using-npm/registry)
* [npm adduser](/commands/npm-adduser)
