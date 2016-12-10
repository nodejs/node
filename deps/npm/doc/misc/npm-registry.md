npm-registry(7) -- The JavaScript Package Registry
==================================================

## DESCRIPTION

To resolve packages by name and version, npm talks to a registry website
that implements the CommonJS Package Registry specification for reading
package info.

Additionally, npm's package registry implementation supports several
write APIs as well, to allow for publishing packages and managing user
account information.

The official public npm registry is at <https://registry.npmjs.org/>.  It
is powered by a CouchDB database, of which there is a public mirror at
<https://skimdb.npmjs.com/registry>.  The code for the couchapp is
available at <https://github.com/npm/npm-registry-couchapp>.

The registry URL used is determined by the scope of the package (see
`npm-scope(7)`). If no scope is specified, the default registry is used, which is
supplied by the `registry` config parameter.  See `npm-config(1)`,
`npmrc(5)`, and `npm-config(7)` for more on managing npm's configuration.

## Can I run my own private registry?

Yes!

The easiest way is to replicate the couch database, and use the same (or
similar) design doc to implement the APIs.

If you set up continuous replication from the official CouchDB, and then
set your internal CouchDB as the registry config, then you'll be able
to read any published packages, in addition to your private ones, and by
default will only publish internally. 

If you then want to publish a package for the whole world to see, you can
simply override the `--registry` option for that `publish` command.

## I don't want my package published in the official registry. It's private.

Set `"private": true` in your package.json to prevent it from being
published at all, or
`"publishConfig":{"registry":"http://my-internal-registry.local"}`
to force it to be published only to your internal registry.

See `package.json(5)` for more info on what goes in the package.json file.

## Will you replicate from my registry into the public one?

No.  If you want things to be public, then publish them into the public
registry using npm.  What little security there is would be for nought
otherwise.

## Do I have to use couchdb to build a registry that npm can talk to?

No, but it's way easier.  Basically, yes, you do, or you have to
effectively implement the entire CouchDB API anyway.

## Is there a website or something to see package docs and such?

Yes, head over to <https://npmjs.com/>

## SEE ALSO

* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-developers(7)
* npm-disputes(7)
