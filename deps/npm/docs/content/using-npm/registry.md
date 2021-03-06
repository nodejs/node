---
title: registry
section: 7
description: The JavaScript Package Registry
---

### Description

To resolve packages by name and version, npm talks to a registry website
that implements the CommonJS Package Registry specification for reading
package info.

npm is configured to use the **npm public registry** at
<https://registry.npmjs.org> by default. Use of the npm public registry is
subject to terms of use available at <https://www.npmjs.com/policies/terms>.

You can configure npm to use any compatible registry you like, and even run
your own registry. Use of someone else's registry may be governed by their
terms of use.

npm's package registry implementation supports several
write APIs as well, to allow for publishing packages and managing user
account information.

The npm public registry is powered by a CouchDB database,
of which there is a public mirror at <https://skimdb.npmjs.com/registry>.

The registry URL used is determined by the scope of the package (see
[`scope`](/using-npm/scope). If no scope is specified, the default registry is used, which is
supplied by the `registry` config parameter.  See [`npm config`](/commands/npm-config),
[`npmrc`](/configuring-npm/npmrc), and [`config`](/using-npm/config) for more on managing npm's configuration.

### Does npm send any information about me back to the registry?

Yes.

When making requests of the registry npm adds two headers with information
about your environment:

* `Npm-Scope` – If your project is scoped, this header will contain its
  scope. In the future npm hopes to build registry features that use this
  information to allow you to customize your experience for your
  organization.
* `Npm-In-CI` – Set to "true" if npm believes this install is running in a
  continuous integration environment, "false" otherwise. This is detected by
  looking for the following environment variables: `CI`, `TDDIUM`,
  `JENKINS_URL`, `bamboo.buildKey`. If you'd like to learn more you may find
  the [original PR](https://github.com/npm/npm-registry-client/pull/129)
  interesting.
  This is used to gather better metrics on how npm is used by humans, versus
  build farms.

The npm registry does not try to correlate the information in these headers
with any authenticated accounts that may be used in the same requests.

### How can I prevent my package from being published in the official registry?

Set `"private": true` in your `package.json` to prevent it from being
published at all, or
`"publishConfig":{"registry":"http://my-internal-registry.local"}`
to force it to be published only to your internal/private registry.

See [`package.json`](/configuring-npm/package-json) for more info on what goes in the package.json file.

### Where can I find my own, & other's, published packages?

<https://www.npmjs.com/>

### See also

* [npm config](/commands/npm-config)
* [config](/using-npm/config)
* [npmrc](/configuring-npm/npmrc)
* [npm developers](/using-npm/developers)
