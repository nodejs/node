# Node.js Distribution Policy

This document describes some policies around what is and is not included in the
Node.js distribution.

## Inclusion

Node.js includes some external projects that the Node.js team does not maintain.
The fact of a project's inclusion should not imply anything about the project
relative to its competitors; in some cases, a project was added when it had no
competitors. While the Node.js project supports and encourages competition in the
JavaScript ecosystem, as a policy, the Node.js project does not include multiple
dependencies or tools that serve the same purpose.

The following user-accessible external projects are the ones chosen for their
particular purposes:

* JavaScript engine: V8
* Package manager: `npm`

Being user-accessible, removal or replacement of these projects could happen
only as a semver-major change. In addition, Node.js includes external projects
as internal dependencies. These may be replaced or removed at any time, provided
that doing so is not a breaking change.
