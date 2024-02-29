# Node.js Distribution Policy

This document describes some policies around what is and is not included in the
Node.js distribution.

## Inclusion

Node.js includes many dependencies, such as V8, Undici and others; and comes
bundled with standalone tools such as `npm`. It is not a goal of the Node.js
project to provide a level playing field for all dependencies or bundled tools.
With limited resources, the Node.js project does not intend to bundle or
officially support multiple dependencies or tools that serve the same purpose.

## Package managers

The Node.js distribution includes `npm`, and does not include any other
package managers.
