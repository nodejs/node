---
title: npm-help-search
section: 1
description: Search npm help documentation
---

### Synopsis

```bash
npm help-search <text>
```

### Description

This command will search the npm markdown documentation files for the terms
provided, and then list the results, sorted by relevance.

If only one result is found, then it will show that help topic.

If the argument to `npm help` is not a known help topic, then it will call
`help-search`.  It is rarely if ever necessary to call this command
directly.

### Configuration

#### long

* Type: Boolean
* Default: false

If true, the "long" flag will cause help-search to output context around
where the terms were found in the documentation.

If false, then help-search will just list out the help topics found.

### See Also

* [npm](/commands/npm)
* [npm help](/commands/npm-help)
