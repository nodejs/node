# Customization

> Stability: 1.0 - Early development

Node.js has multiple APIs for customizing behavior and can easily be invoked via
[`--import`][]. The package.json of such a plugin would look something like
this:

```json
{
  "name": "example-nodejs-plugin",
  "keywords": [
    "nodejs-plugin"
  ],
  "exports": {
    "register": "./registration.mjs"
  }
}
```

Setting the keyword `nodejs-plugin` is important for users to find the
package (which may be automated). It should also contain the official name(s)
for the support it provides; a plugin extending Node.js support for TypeScript
should contain `typescript` in its keywords; a customization package that does
not extend Node.js support for TypeScript should not contain `typescript` (such
as when the library is itself merely written in TypeScript). Incorrect keywords
will cause a package to be listed for things it does not do, aggravating users.

## Setting up a plugin

These steps configure Node.js with a customization plugin:

1. Install the plugin via your preferred package manager. For finding the
   plugin, package managers provide often a CLI utility, such as [`npm
   search`][], as well as a website.
2. Once installed, in order to get Node.js to use it, you have 2 options:
   * Pass the `--import` directly in the shell command:
   `node --import=example-nodejs-plugin/register" main.mjs`. This option works
   well when the command is simple, but it can
   quickly become unwieldy as the list of flags grows, and it requires you to
   remember everything you need to pass.
   * Create a [`.env`][`--env-file`] file containing an [`--import`][] for your
   chosen plugin, like `NODE_OPTIONS="--import=example-nodejs-plugin/register"`.
   This option avoids the command becoming unwieldly, and it's easier to
   remember (a single, unchanging `--env-file` flag).
      * Include the `.env` file flag on subsequent runs, like
      `node --env-file=.env main.mjs`.

## APIs

Customizable systems each expose a `register` function, whereby the
customization plugin is registered into Node.js.

* [`node:module`][module.register]

[`"exports"`]: packages.md#exports
[`--env-file`]: cli.md#--env-file
[`--import`]: cli.md#--import
[`npm search`]: https://docs.npmjs.com/cli/v10/commands/npm-search
[module.register]: module.md#moduleregisterspecifier-parenturl-options
