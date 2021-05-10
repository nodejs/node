---
title: npm-view
section: 1
description: View registry info
---

### Synopsis

```bash
npm view [<@scope>/]<name>[@<version>] [<field>[.<subfield>]...]

aliases: info, show, v
```

### Description

This command shows data about a package and prints it to stdout.

As an example, to view information about the `connect` package from the registry, you would run:

```bash
npm view connect
```

The default version is `"latest"` if unspecified.

Field names can be specified after the package descriptor.
For example, to show the dependencies of the `ronn` package at version
`0.3.5`, you could do the following:

```bash
npm view ronn@0.3.5 dependencies
```

You can view child fields by separating them with a period.
To view the git repository URL for the latest version of `npm`, you would run the following command:

```bash
npm view npm repository.url
```

This makes it easy to view information about a dependency with a bit of
shell scripting. For example, to view all the data about the version of
`opts` that `ronn` depends on, you could write the following:

```bash
npm view opts@$(npm view ronn dependencies.opts)
```

For fields that are arrays, requesting a non-numeric field will return
all of the values from the objects in the list. For example, to get all
the contributor names for the `express` package, you would run:

```bash
npm view express contributors.email
```

You may also use numeric indices in square braces to specifically select
an item in an array field. To just get the email address of the first
contributor in the list, you can run:

```bash
npm view express contributors[0].email
```

Multiple fields may be specified, and will be printed one after another.
For example, to get all the contributor names and email addresses, you
can do this:

```bash
npm view express contributors.name contributors.email
```

"Person" fields are shown as a string if they would be shown as an
object.  So, for example, this will show the list of `npm` contributors in
the shortened string format.  (See [`package.json`](/configuring-npm/package-json) for more on this.)

```bash
npm view npm contributors
```

If a version range is provided, then data will be printed for every
matching version of the package.  This will show which version of `jsdom`
was required by each matching version of `yui3`:

```bash
npm view yui3@'>0.5.4' dependencies.jsdom
```

To show the `connect` package version history, you can do
this:

```bash
npm view connect versions
```

### Configuration

#### json

Show information in JSON format. See [`Output`](#output) below.

#### workspaces

Enables workspaces context while searching the `package.json` in the
current folder.  Information about packages named in each workspace will
be viewed.

#### workspace

Enables workspaces context and limits results to only those specified by
this config item.  Only the information about packages named in the
workspaces given here will be viewed.


### Output

If only a single string field for a single version is output, then it
will not be colorized or quoted, to enable piping the output to
another command. If the field is an object, it will be output as a JavaScript object literal.

If the `--json` flag is given, the outputted fields will be JSON.

If the version range matches multiple versions then each printed value
will be prefixed with the version it applies to.

If multiple fields are requested, then each of them is prefixed with
the field name.

### See Also

* [npm search](/commands/npm-search)
* [npm registry](/using-npm/registry)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [npm docs](/commands/npm-docs)
