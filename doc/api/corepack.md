# Corepack

<!-- introduced_in=REPLACEME -->
<!-- type=misc -->

> Stability: 1 - Experimental

_[Corepack][]_ is an experimental tool shipped with Node.js to help with
managing versions of your package managers. It exposes binary proxies for
[Yarn][] and [pnpm][] that, when called, locate the package managers configured
for your local projects, transparently install them, and finally run them
without requiring explicit user interaction.

This feature allows you to make sure that everyone in your team is using
exactly the package manager version you want them to without having to check
them into your repository or managing complex migrations across developers'
systems.

## Workflows

### Enabling the feature

Due to its experimental status Corepack currently needs to be explicitly
enabled to have any effect. To do that simply run [`corepack enable`][], which
will set up the symlinks in your environment, next to the `node` binary
(and overwrite the existing symlinks if necessary).

From this point forward, any call to the `yarn`, `pnpm`, or `pnpx` binaries
will work without further setup. Should you experience a problem, just run
[`corepack disable`][] to remove the proxies from your system (and consider
opening up an issue on the [Corepack repository][] to let us know).

### Configuring a package

The Corepack proxies will find the closest [`package.json`][] files in your
directory hierarchy to extract their [`"packageManager"`][] property.

If the requested package manager is [supported][], Corepack will make sure that
all calls to the relevant binaries are run against the requested version.

### Upgrading the global versions

When running outside of an existing project (for example when running
`yarn init`), Corepack will by default use predefined versions roughly
corresponding to the latest stable releases from each tool. Those versions can
be easily overriden by running the [`corepack prepare`][] command along with the
package manager version you wish to set:

```bash
corepack prepare yarn@x.y.z --activate
```

### Offline workflow

Many production environments don't have network accesses. Since Corepack
usually downloads the package manager releases straight from their registries,
it can conflict with such environments. To avoid that happening, call the
[`corepack prepare`][] command while you still have network access (typically at
the same time you're preparing your deploy image). This will ensure that the
required package managers are available even without network access.

The `prepare` command has [various flags][], consult the detailed
[Corepack documentation][] for more information on the matter.

## Supported package managers

Both [Yarn][] and [pnpm][] are supported by Corepack.

## Troubleshooting

### Running `npm install -g yarn` doesn't work

npm prevents accidentally overriding the Corepack binaries when doing a global
install. To avoid this problem, consider one of the following options:

* Don't run this command anymore; Corepack will provide the package manager
binaries anyway and will ensure that the requested versions are always
available, so installing the package managers explicitly isn't needed anymore.

* Add the `--force` to `npm install`; this will tell npm that it's fine to
override binaries, but you'll erase the Corepack ones in the process (should
that happen, run [`corepack enable`][] again to add them back).

[Corepack]: https://github.com/nodejs/corepack
[Corepack documentation]: https://github.com/nodejs/corepack#readme
[Corepack repository]: https://github.com/nodejs/corepack
[Yarn]: https://yarnpkg.com
[`"packageManager"`]: packages.md#packages_packagemanager
[`corepack disable`]: https://github.com/nodejs/corepack#corepack-disable--name
[`corepack enable`]: https://github.com/nodejs/corepack#corepack-enable--name
[`corepack prepare`]: https://github.com/nodejs/corepack#corepack-prepare--nameversion
[`package.json`]: packages.md
[pnpm]: https://pnpm.js.org
[supported]: #supported-package-managers
[various flags]: https://github.com/nodejs/corepack#utility-commands
