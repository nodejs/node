---
title: npm-fund
section: 1
description: Retrieve funding information
---

### Synopsis

```bash
npm fund [<pkg>]
npm fund [-w <workspace-name>]
```

### Description

This command retrieves information on how to fund the dependencies of a
given project. If no package name is provided, it will list all
dependencies that are looking for funding in a tree structure, listing the
type of funding and the url to visit. If a package name is provided then it
tries to open its funding url using the `--browser` config param; if there
are multiple funding sources for the package, the user will be instructed
to pass the `--which` option to disambiguate.

The list will avoid duplicated entries and will stack all packages that
share the same url as a single entry. Thus, the list does not have the same
shape of the output from `npm ls`.

#### Example

### Workspaces support

It's possible to filter the results to only include a single workspace and its
dependencies using the `workspace` config option.

#### Example:

Here's an example running `npm fund` in a project with a configured
workspace `a`:

```bash
$ npm fund
test-workspaces-fund@1.0.0
+-- https://example.com/a
| | `-- a@1.0.0
| `-- https://example.com/maintainer
|     `-- foo@1.0.0
+-- https://example.com/npmcli-funding
|   `-- @npmcli/test-funding
`-- https://example.com/org
    `-- bar@2.0.0
```

And here is an example of the expected result when filtering only by
a specific workspace `a` in the same project:

```bash
$ npm fund -w a
test-workspaces-fund@1.0.0
`-- https://example.com/a
  | `-- a@1.0.0
  `-- https://example.com/maintainer
      `-- foo@2.0.0
```

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm fund` command to open websites.

#### json

* Type: Boolean
* Default: false

Show information in JSON format.

#### unicode

* Type: Boolean
* Default: true

Whether to represent the tree structure using unicode characters.
Set it to `false` in order to use all-ansi output.

#### `workspace`

* Default:
* Type: String (can be set multiple times)

Enable running a command in the context of the configured workspaces of the
current project while filtering by running only the workspaces defined by
this configuration option.

Valid values for the `workspace` config are either:
* Workspace names
* Path to a workspace directory
* Path to a parent workspace directory (will result to selecting all of the
nested workspaces)

This value is not exported to the environment for child processes.

#### which

* Type: Number
* Default: undefined

If there are multiple funding sources, which 1-indexed source URL to open.

## See Also

* [npm install](/commands/npm-install)
* [npm docs](/commands/npm-docs)
* [npm ls](/commands/npm-ls)
* [npm config](/commands/npm-config)
* [npm workspaces](/using-npm/workspaces)
