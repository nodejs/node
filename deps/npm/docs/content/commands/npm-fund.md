---
title: npm-fund
section: 1
description: Retrieve funding information
---

### Synopsis

```bash
npm fund [<package-spec>]
```

### Description

This command retrieves information on how to fund the dependencies of a
given project. If no package name is provided, it will list all
dependencies that are looking for funding in a tree structure, listing
the type of funding and the url to visit. If a package name is provided
then it tries to open its funding url using the
[`--browser` config](/using-npm/config#browser) param; if there are multiple
funding sources for the package, the user will be instructed to pass the
`--which` option to disambiguate.

The list will avoid duplicated entries and will stack all packages that
share the same url as a single entry. Thus, the list does not have the
same shape of the output from `npm ls`.

#### Example

### Workspaces support

It's possible to filter the results to only include a single workspace
and its dependencies using the
[`workspace` config](/using-npm/config#workspace) option.

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

And here is an example of the expected result when filtering only by a
specific workspace `a` in the same project:

```bash
$ npm fund -w a
test-workspaces-fund@1.0.0
`-- https://example.com/a
  | `-- a@1.0.0
  `-- https://example.com/maintainer
      `-- foo@2.0.0
```

### Configuration

#### `json`

* Default: false
* Type: Boolean

Whether or not to output JSON data, rather than the normal output.

* In `npm pkg set` it enables parsing set values with JSON.parse() before
  saving them to your `package.json`.

Not supported by all npm commands.



#### `browser`

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: null, Boolean, or String

The browser that is called by npm commands to open websites.

Set to `false` to suppress browser behavior and instead print urls to
terminal.

Set to `true` to use default system URL opener.



#### `unicode`

* Default: false on windows, true on mac/unix systems with a unicode locale,
  as defined by the `LC_ALL`, `LC_CTYPE`, or `LANG` environment variables.
* Type: Boolean

When set to true, npm uses unicode characters in the tree output. When
false, it uses ascii characters instead of unicode glyphs.



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

#### `which`

* Default: null
* Type: null or Number

If there are multiple funding sources, which 1-indexed source URL to open.



## See Also

* [package spec](/using-npm/package-spec)
* [npm install](/commands/npm-install)
* [npm docs](/commands/npm-docs)
* [npm ls](/commands/npm-ls)
* [npm config](/commands/npm-config)
* [npm workspaces](/using-npm/workspaces)
