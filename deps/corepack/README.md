# <img src="./icon.svg" height="25" /> corepack

Corepack is a zero-runtime-dependency Node.js script that acts as a bridge between Node.js projects and the package managers they are intended to be used with during development. In practical terms, **Corepack will let you use Yarn and pnpm without having to install them** - just like what currently happens with npm, which is shipped by Node.js by default.

**Important:** At the moment, Corepack only covers Yarn and pnpm. Given that we have little control on the npm project, we prefer to focus on the Yarn and pnpm use cases. As a result, Corepack doesn't have any effect at all on the way you use npm.

## How to Install

### Default Installs

Corepack is distributed by default with Node.js 14.19.0 and 16.9.0, but is opt-in for the time being. Run `corepack enable` to install the required shims.

### Manual Installs

<details>
<summary>Click here to see how to install Corepack using npm</summary>

First uninstall your global Yarn and pnpm binaries (just leave npm). In general, you'd do this by running the following command:

```shell
npm uninstall -g yarn pnpm

# That should be enough, but if you installed Yarn without going through npm it might
# be more tedious - for example, you might need to run `brew uninstall yarn` as well.
```

Then install Corepack:

```shell
npm install -g corepack
```

We do acknowledge the irony and overhead of using npm to install Corepack, which is at least part of why the preferred option is to use the Corepack version that is distributed along with Node.js itself.

</details>

## Usage

### When Building Packages

Just use your package managers as you usually would. Run `yarn install` in Yarn projects, `pnpm install` in pnpm projects, and `npm` in npm projects. Corepack will catch these calls, and depending on the situation:

- **If the local project is configured for the package manager you're using**, Corepack will silently download and cache the latest compatible version.

- **If the local project is configured for a different package manager**, Corepack will request you to run the command again using the right package manager - thus avoiding corruptions of your install artifacts.

- **If the local project isn't configured for any package manager**, Corepack will assume that you know what you're doing, and will use whatever package manager version has been pinned as "known good release". Check the relevant section for more details.

### When Authoring Packages

Set your package's manager with the `packageManager` field in `package.json`:

```json
{
  "packageManager": "yarn@3.2.3+sha224.953c8233f7a92884eee2de69a1b92d1f2ec1655e66d08071ba9a02fa"
}
```

Here, `yarn` is the name of the package manager, specified at version `3.2.3`, along with the SHA-224 hash of this version for validation. `packageManager@x.y.z` is required. The hash is optional but strongly recommended as a security practice. Permitted values for the package manager are `yarn`, `npm`, and `pnpm`.

## Known Good Releases

When running Corepack within projects that don't list a supported package
manager, it will default to a set of Known Good Releases. In a way, you can
compare this to Node.js, where each version ships with a specific version of npm.

If there is no Known Good Release for the requested package manager, Corepack
looks up the npm registry for the latest available version and cache it for
future use.

The Known Good Releases can be updated system-wide using the `--activate` flag
from the `corepack prepare` and `corepack hydrate` commands.

## Offline Workflow

The utility commands detailed in the next section.

- Either you can use the network while building your container image, in which case you'll simply run `corepack prepare` to make sure that your image includes the Last Known Good release for the specified package manager.

  - If you want to have *all* Last Known Good releases for all package managers, just use the `--all` flag which will do just that.

- Or you're publishing your project to a system where the network is unavailable, in which case you'll preemptively generate a package manager archive from your local computer (using `corepack prepare -o`) before storing it somewhere your container will be able to access (for example within your repository). After that it'll just be a matter of running `corepack hydrate <path/to/corepack.tgz>` to setup the cache.

## Utility Commands

### `corepack <binary name>[@<version>] [... args]`

This meta-command runs the specified package manager in the local folder. You can use it to force an install to run with a given version, which can be useful when looking for regressions.

Note that those commands still check whether the local project is configured for the given package manager (ie you won't be able to run `corepack yarn install` on a project where the `packageManager` field references `pnpm`).

### `corepack enable [... name]`

| Option | Description |
| --- | --- |
| `--install-directory` | Add the shims to the specified location |

This command will detect where Node.js is installed and will create shims next to it for each of the specified package managers (or all of them if the command is called without parameters). Note that the npm shims will not be installed unless explicitly requested, as npm is currently distributed with Node.js through other means.

### `corepack disable [... name]`

| Option | Description |
| --- | --- |
| `--install-directory` | Remove the shims to the specified location |

This command will detect where Node.js is installed and will remove the shims from there.

### `corepack prepare [... name@version]`

| Option | Description |
| --- | --- |
| `--all` | Prepare the "Last Known Good" version of all supported package managers |
| `-o,--output` | Also generate an archive containing the package managers |
| `--activate` | Also update the "Last Known Good" release |

This command will download the given package managers (or the one configured for the local project if no argument is passed in parameter) and store it within the Corepack cache. If the `-o,--output` flag is set (optionally with a path as parameter), an archive will also be generated that can be used by the `corepack hydrate` command.

### `corepack hydrate <path/to/corepack.tgz>`

| Option | Description |
| --- | --- |
| `--activate` | Also update the "Last Known Good" release |

This command will retrieve the given package manager from the specified archive and will install it within the Corepack cache, ready to be used without further network interaction.

## Environment Variables

- `COREPACK_DEFAULT_TO_LATEST` can be set to `0` in order to instruct Corepack
  not to lookup on the remote registry for the latest version of the selected
  package manager.

- `COREPACK_ENABLE_NETWORK` can be set to `0` to prevent Corepack from accessing
  the network (in which case you'll be responsible for hydrating the package
  manager versions that will be required for the projects you'll run, using
  `corepack hydrate`).

- `COREPACK_ENABLE_STRICT` can be set to `0` to prevent Corepack from throwing error
  if the package manager does not correspond to the one defined for the current project.
  This means that if a user is using the package manager specified in the current project,
  it will use the version specified by the project's `packageManager` field.
  But if the user is using other package manager different from the one specified
  for the current project, it will use the system-wide package manager version.

- `COREPACK_ENABLE_PROJECT_SPEC` can be set to `0` to prevent Corepack from checking
  if the package manager corresponds to the one defined for the current project.
  This means that it will always use the system-wide package manager regardless of
  what is being specified in the project's `packageManager` field.

- `COREPACK_HOME` can be set in order to define where Corepack should install
  the package managers. By default it is set to `%LOCALAPPDATA%\node\corepack`
  on Windows, and to `$HOME/.cache/node/corepack` everywhere else.

- `COREPACK_ROOT` has no functional impact on Corepack itself; it's automatically being set in your environment by Corepack when it shells out to the underlying package managers, so that they can feature-detect its presence (useful for commands like `yarn init`).

- `COREPACK_NPM_REGISTRY` sets the registry base url used when retrieving package managers from npm. Default value is `https://registry.npmjs.org`

- `COREPACK_NPM_TOKEN` sets a Bearer token authorization header when connecting to a npm type registry.

- `COREPACK_NPM_USERNAME` and `COREPACK_NPM_PASSWORD` to set a Basic authorization header when connecting to a npm type registry. Note that both environment variables are required and as plain text. If you want to send an empty password, explicitly set `COREPACK_NPM_PASSWORD` to an empty string.

- `HTTP_PROXY`, `HTTPS_PROXY`, and `NO_PROXY` are supported through [`node-proxy-agent`](https://github.com/TooTallNate/node-proxy-agent).

## Contributing

See [`CONTRIBUTING.md`](./CONTRIBUTING.md).

## Design

See [`DESIGN.md`](/DESIGN.md).

## License (MIT)

See [`LICENSE.md`](./LICENSE.md).
