# <img src="./icon.svg" height="25" /> corepack

Corepack is a zero-runtime-dependency Node script that acts as a bridge between Node projects and the package managers they are intended to be used with during development. In practical terms, **Corepack will let you use Yarn and pnpm without having to install them** - just like what currently happens with npm, which is shipped by Node by default.

**Important:** At the moment, Corepack only covers Yarn and pnpm. Given that we have little control on the npm project, we prefer to focus on the Yarn and pnpm use cases. As a result, Corepack doesn't have any effect at all on the way you use npm.

## How to Install

### Default Installs

Corepack isn't intended to be installed manually. While it's certainly possible, we're working with the Node TSC to provide Corepack by default starting from Node 15, thus ensuring that all package managers can be used with little to no friction.

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

We do acknowledge the irony and overhead of using npm to install Corepack, which is at least part of why the preferred option is to use the Corepack version that will be distributed along with Node itself.

</details>

### Prebuilt Binaries

<details>
<summary>Click here to see how to download prebuilt Corepack Node distributions</summary>

We have a few prebuilt Node binaries (based on the [following branch](https://github.com/arcanis/node/tree/mael/pmm)) that you can just download, unpack somewhere, and add to your `PATH` environment variable.

1. Go to [this page](https://github.com/arcanis/pmm/actions?query=workflow%3ABuild)
2. Open the latest build (the one at the top)
3. Download the right artifact (Linux or Darwin)
4. Unzip the artifact, then untar it
5. Add the `node-v15.0.0-nightlyYYYY-MM-DDXXXX-linux-x64/bin` directory to your `$PATH`

</details>

## Usage

Just use your package managers as you usually would. Run `yarn install` in Yarn projects, `pnpm install` in pnpm projects, and `npm` in npm projects. Corepack will catch these calls, and depending on the situation:

- **If the local project is configured for the package manager you're using**, Corepack will silently download and cache the latest compatible version.

- **If the local project is configured for a different package manager**, Corepack will request you to run the command again using the right package manager - thus avoiding corruptions of your install artifacts.

- **If the local project isn't configured for any package manager**, Corepack will assume that you know what you're doing, and will use whatever package manager version has been pinned as "known good release". Check the relevant section for more details.

## Known Good Releases

When running Yarn or pnpm within projects that don't list a supported package manager, Corepack will default to a set of Known Good Releases. In a way, you can compare this to Node, where each version ships with a specific version of npm.

The Known Good Releases can be updated system-wide using the `--activate` flag from the `corepack prepare` and `corepack hydrate` commands.

## Offline Workflow

The utility commands detailed in the next section.

- Either you can use the network while building your container image, in which case you'll simply run `corepack prepare --cache-only <name>` to make sure that your image includes the Last Known Good release for the specified package manager.

  - If you want to have *all* Last Known Good releases for all package managers, just use the `--all` flag which will do just that.

- Or you're publishing your project to a system where the network is unavailable, in which case you'll preemptively generate a package manager archive from your local computer (using `corepack prepare`) before storing it somewhere your container will be able to access (for example within your repository). After that, it's just a matter of running `corepack hydrate <path/to/corepack>` to setup the cache.

## Utility Commands

### `corepack prepare [name@version]`

| Option | Description |
| --- | --- |
| `--all` | Prepare the "Last Known Good" version of all supported package managers |
| `--cache-only` | Just populate the cache, don't generate an archive |
| `--activate` | Also update the "Last Known Good" release |

This command will download the given package manager (or the one configured for the local project if no argument is passed in parameter) and store it within the Corepack cache. Unless the `--cache-only` flag is set, an archive will also be generated that can be used by the `corepack hydrate` command.

### `corepack hydrate <path/to/corepack.tgz>`

| Option | Description |
| --- | --- |
| `--activate` | Also update the "Last Known Good" release |

This command will retrieve the given package manager from the specified archive and will install it within the Corepack cache, ready to be used without further network interaction.

## Contributing

If you want to build corepack yourself things yourself, you can build the project like this:

1. Clone this repository
2. Run `yarn build` (no need for `yarn install`)
3. The `dist/` directory now contains the pmm build and the shims
4. Call `node ./dist/pmm --version` and behold

You can also run the tests with `yarn jest` (still no install needed).

## Design

Various tidbits about Corepack's design are explained in more details in [DESIGN.md](/DESIGN.md).

## License (MIT)

> **Copyright © Corepack contributors**
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
