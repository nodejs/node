# Node.js Distribution Policy

This document describes some policies around what is and is not included in the
Node.js distribution.

## Inclusion

The Node.js distribution includes some external software that the Node.js
project does not maintain. The choice to include a particular piece of software
should not imply anything about that software relative to its competitors; in
some cases, software was added when it had no competitors. While the Node.js
project supports and encourages competition in the JavaScript ecosystem, as a
policy, the Node.js project does not include multiple dependencies or tools that
serve the same purpose.

The following user-accessible external tools or libraries are the ones chosen
for their particular purposes:

* JavaScript engine: V8
* Package manager: `npm`
* Package manager version manager: Corepack

Being user-accessible, removal or replacement of these projects could happen
only as a semver-major change, unless the related feature or project is
documented as experimental. In addition, Node.js includes external projects as
internal dependencies. These may be replaced or removed at any time, provided
that doing so is not a breaking change.

## Placeholder executables

A placeholder executable is a binary, symlink or script that has a name that refers to software that is not distributed with Node.js, but instead would be downloaded when the executable is run.

Adding placeholder executables has a high boundary to be included in Node.js as it has a strong impact on the security side as well as the problem that it provides a strong support for a specific project outside of Node.js, which Node.js tries to avoid. Node.js is not going to support any outside project without a very strong reason to do so.
It therefore requires an affirmative vote of the Node.js Technical Steering Committee to include any such placeholder executables.
