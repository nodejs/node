# Customization

> Stability: 1.0 - Early development

Node.js has multiple APIs for customizing behaviour and can easily be invoked
via [`--import`][]: A `"nodejs-customization"` [entry point][`"exports"`] for a
package loaded via [`--import`][] is automatically run at startup. The
package.json of such a plugin would look something like this:

```json
{
  "name": "example-nodejs-plugin",
  "keywords": [
    "nodejs-plugin"
  ],
  "exports": {
    "nodejs-customization": "./registration.mjs"
  }
}
```

Setting the keyword `nodejs-plugin` is important for users to find the package
(which may be automated). It should also contain the official name(s) for the
support it provides; for a plugin extending support for typescript would contain
`typescript` in its keywords; avoid red-herrings such as `typescript` as a
keyword when the library does not extend support for typescript but is merely
itself written in typescript. Failing to avoid these red-herrings will cause
your package to be listed for things it does not do, aggravating users.

These steps configure Node.js with a customization plugin:

1. Install the plugin via your preferred package manager. For finding the
   plugin, package managers provide often a CLI utility, such as [`npm
   search`][], as well as a website.
2. Once installed, in order to get Node.js to automatically use it, create a
   [`.env`][`--env-file`] file containing an [`--import`][] for your chosen
   plugin, like `NODE_OPTIONS="--import=example-nodejs-plugin"`.
3. Include the env file flag on subsequent runs, like
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
