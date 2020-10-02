---
section: cli-commands 
title: npm-help
description: Get help on npm
---

# npm-help(1)

## Get help on npm

### Synopsis

```bash
npm help <term> [<terms..>]
```

### Description

If supplied a topic, then show the appropriate documentation page.

If the topic does not exist, or if multiple terms are provided, then run
the `help-search` command to find a match.  Note that, if `help-search`
finds a single subject, then it will run `help` on that topic, so unique
matches are equivalent to specifying a topic name.

### Configuration

#### viewer

* Default: "man" on Posix, "browser" on Windows
* Type: path

The program to use to view help content.

Set to `"browser"` to view html help content in the default web browser.

### See Also

* [npm](/cli-commands/npm)
* [npm folders](/configuring-npm/folders)
* [npm config](/cli-commands/config)
* [npmrc](/configuring-npm/npmrc)
* [package.json](/configuring-npm/package-json)
* [npm help-search](/cli-commands/help-search)
