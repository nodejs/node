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
enabled to have any effect. To do that, first make sure that neither Yarn nor
pnpm are installed (uninstall them if needed), then run:

```bash
corepack enable
```

From this point forward, any call to the `yarn`, `pnpm`, or `pnpx` binaries
will work without further setup. Should you experience a problem, just run
`corepack disable` to remove the proxies from your system (and consider opening
up an issue on the [Corepack repository][] to let us know).

### Configuring a project

The Corepack proxies will find the closest `package.json` files in your
directory hierarchy to extract their `packageManager` property, formatted as
such:

```json
{
  "packageManager": "yarn@1.22.10"
}
```

If the requested package manager is supported (only Yarn and pnpm are at the
moment), Corepack will make sure that all calls to the relevant binaries (in
this case any command starting with `yarn`) are ran against the specified
version.

### Upgrading the global versions

When running outside of an existing project (for example when running
`yarn init`), Corepack will by default use predefined versions roughly
corresponding to the latest stable releases from each tool. Those versions can
be easily overriden by running the `corepack prepare` command along with the
package manager version you wish to set:

```bash
corepack prepare yarn@x.y.z --activate
```

### Offline workflow

Many production environments don't have network accesses. Since Corepack
usually downloads the package manager releases straight from their registries,
it can conflict with such environments. To avoid that happening, call the
`corepack prepare` command while you still have network access (typically at
the same time you're preparing your deploy image). This will ensure that the
required package managers are available even without network access.

The `prepare` command has [various flags][], consult the detailed Corepack
documentation for more information on the matter.

## Troubleshooting

### Running `npm install -g yarn` doesn't work

Npm prevents accidentally overriding the Corepack binaries when doing a global
install. To avoid this problem, consider one of the following options:

* Don't run this command anymore; Corepack will provide the package manager
binaries anyway and will ensure that the requested versions are always
available, so installing the package managers explicitly isn't needed anymore.

* Add the `--force` to `npm install`; this will tell npm that it's fine to
override binaries, but you'll erase the Corepack ones in the process (should
that happen, run `corepack enable` again to add them back).

[Corepack]: https://github.com/arcanis/corepack
[Corepack repository]: https://github.com/arcanis/corepack
[Yarn]: https://yarnpkg.com
[pnpm]: https://pnpm.js.org
[various flags]: https://github.com/arcanis/corepack#utility-commands
