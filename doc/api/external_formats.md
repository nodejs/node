# Modules: External formats

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

## Introduction

Node natively understands a handful of formats (see the table in [Modules: Load hook][Load
customization hook]). Non-native / external formats require a translator (under the hood, this is a
custom loader). When attempting to run a module with an external format (ex `node main.ts`), node
will try to detect what it is. At current, when node is able to identify the format, it prints a
message to stdout with instructions (that lead here).

## Setting up a module translator

There are a few, currently manual, steps you need to complete. We intend to automate these in
future.

1. You will need to find and install a module translator via your preferred package manager. Some
   package managers provide a CLI utility as well as a website.
1. Once installed, in order to get node to automatically use it, you will need to create a `.env`
   file containing a [`--import`][CLI import] for your chosen module translator's package (ex `NODE_OPTIONS="--import=example-translator/register"`). See [CLI:
   env file][CLI env file].
1. When you subsequently run node, include the env file flag (ex `node --env-file=.env main.ts`).

## Publishing a module translator

A few specific things are necessary when publishing a module translator.

### Package registration

Your package registration needs the following tags (aka keywords): `module translator`, `node`, and
the offical name(s) of whatever external format(s) (ex `typescript`) your package handles. We plan
for node to eventually search by these tags automatically when a user tries to run an unhandled
external formal; if your package is not registered with the tags, it will not be included in the
list of suggestions presented to the user.

### Package setup

Your package exports must contain a `"register"` key and should also contain keys for the external format(s) it handles, like:

```json
{
  "name": "example-translator",
  "exports": {
    "css": "./css.mjs",
    "register": "registration.mjs",
    "typescript": "./typescript.mjs"
  }
}
```

The filenames (the values in `"exports"`) can be whatever you want (see [package.json entry
points][Package entry points]).

`css.mjs` and `typescript.mjs` from the above example are [custom loaders][Module customization hooks]. They will need a [`resolve`][Resolve customization hook] hook that sets `format` to the
format it handles (ex `'css'` and `'typescript'` respectively), a [`load`][Load customization hook] hook
that translates the source of the external format to something node understands, and optionally an
[`initialize`][Initialize customization hook] hook.

`registration.mjs` registers your loader(s) with node (see [module.register][Module register]). It would look something like:

```mjs
import { register } from 'node:module';

register('example-translator/css');
register('example-translator/typescript');
```

[CLI env file]: cli.md#--env-fileconfig
[CLI import]: cli.md#--import
[Initialize customization hook]: module.md#initialize
[Load customization hook]: module.md#loadurl-context-nextload
[Module customization hooks]: module.md#customization-hooks
[Module register]: module.md#moduleregisterspecifier-parenturl-options
[Package entry points]: packages.md#package-entry-points
[Resolve customization hook]: module.md#resolvespecifier-context-nextresolve
