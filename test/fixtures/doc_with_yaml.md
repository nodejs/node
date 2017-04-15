# Sample Markdown with YAML info

## Foobar
<!-- YAML
added: v1.0.0
-->

Describe `Foobar` in more detail here.

## Foobar II
<!-- YAML
added:
  - v5.3.0
  - v4.2.0
changes:
  - version: v4.2.0
    pr-url: https://github.com/nodejs/node/pull/3276
    description: The `error` parameter can now be an arrow function.
-->

Describe `Foobar II` in more detail here. fg(1)

## Deprecated thingy
<!-- YAML
added: v1.0.0
deprecated: v2.0.0
-->

Describe `Deprecated thingy` in more detail here. fg(1p)

## Something
<!-- This is not a metadata comment -->

Describe `Something` in more detail here.
