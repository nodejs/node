---
title: scripts
section: 7
description: How npm handles the "scripts" field
---

### Description

The `"scripts"` property of your `package.json` file supports a number of built-in scripts and their preset life cycle events as well as arbitrary scripts.
These all can be executed by running `npm run <stage>`.
*Pre* and *post* commands with matching names will be run for those as well (e.g.
`premyscript`,
`myscript`, `postmyscript`).
Scripts from dependencies can be run with `npm explore <pkg> -- npm run <stage>`.

### Pre & Post Scripts

To create "pre" or "post" scripts for any scripts defined in the
`"scripts"` section of the `package.json`, simply create another script
*with a matching name* and add "pre" or "post" to the beginning of them.

```json
{
  "scripts": {
    "precompress": "{{ executes BEFORE the `compress` script }}",
    "compress": "{{ run command to compress files }}",
    "postcompress": "{{ executes AFTER `compress` script }}"
  }
}
```

In this example `npm run compress` would execute these scripts as described.

### Life Cycle Scripts

There are some special life cycle scripts that happen only in certain situations.
These scripts happen in addition to the `pre<event>`, `post<event>`, and
`<event>` scripts.

* `prepare`, `prepublish`, `prepublishOnly`, `prepack`, `postpack`, `dependencies`

**prepare** (since `npm@4.0.0`)
* Runs BEFORE the package is packed, i.e.
during `npm publish` and `npm pack`
* Runs on local `npm install` without package arguments (runs with flags like `--production` or `--omit=dev`, but does not run when installing specific packages like `npm install express`)
* Runs AFTER `prepublishOnly` and `prepack`, but BEFORE `postpack`
* Runs for a package if it's being installed as a link through `npm install <folder>`

* NOTE: If a package being installed through git contains a `prepare` script, its `dependencies` and `devDependencies` will be installed, and the prepare script will be run, before the package is packaged and installed.

* As of `npm@7` these scripts run in the background.
  To see the output, run with: `--foreground-scripts`.

* **In workspaces, prepare scripts run concurrently** across all packages. If you have interdependent packages where one must build before another, consider using `--foreground-scripts` (which can be set in `.npmrc` with `foreground-scripts=true`) to run scripts sequentially, or structure your build differently.

**prepublish** (DEPRECATED)
* Does not run during `npm publish`, but does run during `npm ci` and `npm install`.
See below for more info.

**prepublishOnly**
* Runs BEFORE the package is prepared and packed, ONLY on `npm publish`.

**prepack**
* Runs BEFORE a tarball is packed (on "`npm pack`", "`npm publish`", and when installing a git dependency).
* NOTE: "`npm run pack`" is NOT the same as "`npm pack`". "`npm run pack`" is an arbitrary user defined script name, whereas, "`npm pack`" is a CLI defined command.

**postpack**
* Runs AFTER the tarball has been generated but before it is moved to its final destination (if at all, publish does not save the tarball locally)

**dependencies**
* Runs AFTER any operations that modify the `node_modules` directory IF changes occurred.
* Does NOT run in global mode

#### Prepare and Prepublish

**Deprecation Note: prepublish**

Since `npm@1.1.71`, the npm CLI has run the `prepublish` script for both `npm publish` and `npm install`, because it's a convenient way to prepare a package for use (some common use cases are described in the section below).
It has also turned out to be, in practice, [very confusing](https://github.com/npm/npm/issues/10074).
As of `npm@4.0.0`, a new event has been introduced, `prepare`, that preserves this existing behavior.
A _new_ event, `prepublishOnly` has been added as a transitional strategy to allow users to avoid the confusing behavior of existing npm versions and only run on `npm publish` (for instance, running the tests one last time to ensure they're in good shape).

See <https://github.com/npm/npm/issues/10074> for a much lengthier justification, with further reading, for this change.

**Use Cases**

Use a `prepare` script to perform build tasks that are platform-independent and need to run before your package is used.
This includes tasks such as:

* Compiling TypeScript or other source code into JavaScript.
* Creating minified versions of JavaScript source code.
* Fetching remote resources that your package will use.

Running these build tasks in the `prepare` script ensures they happen once, in a single place, reducing complexity and variability.
Additionally, this means that:

* You can depend on build tools as `devDependencies`, and thus your users don't need to have them installed.
* You don't need to include minifiers in your package, reducing the size for your users.
* You don't need to rely on your users having `curl` or `wget` or other system tools on the target machines.

#### Dependencies

The `dependencies` script is run any time an `npm` command causes changes to the `node_modules` directory.
It is run AFTER the changes have been applied and the `package.json` and `package-lock.json` files have been updated.

### Life Cycle Operation Order

#### [`npm cache add`](/commands/npm-cache)

* `prepare`

#### [`npm ci`](/commands/npm-ci)

* `preinstall`
* `install`
* `postinstall`
* `prepublish`
* `preprepare`
* `prepare`
* `postprepare`

These all run after the actual installation of modules into
 `node_modules`, in order, with no internal actions happening in between

#### [`npm diff`](/commands/npm-diff)

* `prepare`

#### [`npm install`](/commands/npm-install)

These also run when you run `npm install -g <pkg-name>`

* `preinstall`
* `install`
* `postinstall`
* `prepublish`
* `preprepare`
* `prepare`
* `postprepare`

If there is a `binding.gyp` file in the root of your package and you haven't defined your own `install` or `preinstall` scripts, npm will default the `install` command to compile using node-gyp via `node-gyp rebuild`

These are run from the scripts of `<pkg-name>`

#### [`npm pack`](/commands/npm-pack)

* `prepack`
* `prepare`
* `postpack`

#### [`npm publish`](/commands/npm-publish)

* `prepublishOnly`
* `prepack`
* `prepare`
* `postpack`
* `publish`
* `postpublish`

#### [`npm rebuild`](/commands/npm-rebuild)

* `preinstall`
* `install`
* `postinstall`
* `prepare`

`prepare` is only run if the current directory is a symlink (e.g.
with linked packages)

#### [`npm restart`](/commands/npm-restart)

If there is a `restart` script defined, these events are run; otherwise,
`stop` and `start` are both run if present, including their `pre` and `post` iterations)

* `prerestart`
* `restart`
* `postrestart`

#### [`npm run <user defined>`](/commands/npm-run)

* `pre<user-defined>`
* `<user-defined>`
* `post<user-defined>`

#### [`npm start`](/commands/npm-start)

* `prestart`
* `start`
* `poststart`

If there is a `server.js` file in the root of your package, then npm will default the `start` command to `node server.js`.
`prestart` and `poststart` will still run in this case.

#### [`npm stop`](/commands/npm-stop)

* `prestop`
* `stop`
* `poststop`

#### [`npm test`](/commands/npm-test)

* `pretest`
* `test`
* `posttest`

#### [`npm version`](/commands/npm-version)

* `preversion`
* `version`
* `postversion`

#### A Note on a lack of [`npm uninstall`](/commands/npm-uninstall) scripts

While npm v6 had `uninstall` lifecycle scripts, npm v7 does not.
Removal of a package can happen for a wide variety of reasons, and there's no clear way to currently give the script enough context to be useful.


Reasons for a package removal include:

* a user directly uninstalled this package
* a user uninstalled a dependent package and so this dependency is being uninstalled
* a user uninstalled a dependent package but another package also depends on this version
* this version has been merged as a duplicate with another version
* etc.

Due to the lack of necessary context, `uninstall` lifecycle scripts are not implemented and will not function.

### Working Directory for Scripts

Scripts are always run from the root of the package folder, regardless of what the current working directory is when `npm` is invoked.
This means your scripts can reliably assume they are running in the package root.

If you want your script to behave differently based on the directory you were in when you ran `npm`, you can use the `INIT_CWD` environment variable, which holds the full path you were in when you ran `npm run`.

#### Historical Behavior in Older npm Versions

For npm v6 and earlier, scripts were generally run from the root of the package, but there were rare cases and bugs in older versions where this was not guaranteed.
If your package must support very old npm versions, you may wish to add a safeguard in your scripts (for example, by checking process.cwd()).

For more details, see:
- [npm v7 release notes](https://github.com/npm/cli/releases/tag/v7.0.0)
- [Discussion about script working directory reliability in npm v6 and earlier](https://github.com/npm/npm/issues/12356)

### User

When npm is run as root, scripts are always run with the effective uid and gid of the working directory owner.

### Environment

Package scripts run in an environment where many pieces of information are made available regarding the setup of npm and the current state of the process.

#### path

If you depend on modules that define executable scripts, like test suites, then those executables will be added to the `PATH` for executing the scripts.
So, if your package.json has this:

```json
{
  "name" : "foo",
  "dependencies" : {
    "bar" : "0.1.x"
  },
  "scripts": {
    "start" : "bar ./test"
  }
}
```

then you could run `npm start` to execute the `bar` script, which is exported into the `node_modules/.bin` directory on `npm install`.

#### package.json vars

npm sets the following environment variables from the package.json:

* `npm_package_name` - The package name
* `npm_package_version` - The package version
* `npm_package_bin_*` - Each executable defined in the bin field
* `npm_package_engines_*` - Each engine defined in the engines field
* `npm_package_config_*` - Each config value defined in the config field
* `npm_package_json` - The full path to the package.json file

Additionally, for install scripts (`preinstall`, `install`, `postinstall`), npm sets these environment variables:

* `npm_package_resolved` - The resolved URL for the package
* `npm_package_integrity` - The integrity hash for the package
* `npm_package_optional` - Set to `"true"` if the package is optional
* `npm_package_dev` - Set to `"true"` if the package is a dev dependency
* `npm_package_peer` - Set to `"true"` if the package is a peer dependency
* `npm_package_dev_optional` - Set to `"true"` if the package is both dev and optional

For example, if you had `{"name":"foo", "version":"1.2.5"}` in your package.json file, then your package scripts would have the `npm_package_name` environment variable set to "foo", and the `npm_package_version` set to "1.2.5". You can access these variables in your code with `process.env.npm_package_name` and `process.env.npm_package_version`.

**Note:** In npm 7 and later, most package.json fields are no longer provided as environment variables. Scripts that need access to other package.json fields should read the package.json file directly. The `npm_package_json` environment variable provides the path to the file for this purpose.

See [`package.json`](/configuring-npm/package-json) for more on package configs.

#### current lifecycle event

Lastly, the `npm_lifecycle_event` environment variable is set to whichever stage of the cycle is being executed.
So, you could have a single script used for different parts of the process which switches based on what's currently happening.

Objects are flattened following this format, so if you had
`{"scripts":{"install":"foo.js"}}` in your package.json, then you'd see this in the script:

```bash
process.env.npm_package_scripts_install === "foo.js"
```

### Examples

For example, if your package.json contains this:

```json
{
  "scripts" : {
    "prepare" : "scripts/build.js",
    "test" : "scripts/test.js"
  }
}
```

then `scripts/build.js` will be called for the prepare stage of the lifecycle, and you can check the
`npm_lifecycle_event` environment variable if your script needs to behave differently in different contexts.

If you want to run build commands, you can do so.
This works just fine:

```json
{
  "scripts" : {
    "prepare" : "npm run build",
    "build" : "tsc",
    "test" : "jest"
  }
}
```

### Exiting

Scripts are run by passing the line as a script argument to `/bin/sh` on POSIX systems or `cmd.exe` on Windows.
You can control which shell is used by setting the [`script-shell`](/using-npm/config#script-shell) configuration option.

If the script exits with a code other than 0, then this will abort the process.

Note that these script files don't have to be Node.js or even JavaScript programs.
They just have to be some kind of executable file.

### Best Practices

* Don't exit with a non-zero error code unless you *really* mean it.
  If the failure is minor or only will prevent some optional features, then it's better to just print a warning and exit successfully.
* Try not to use scripts to do what npm can do for you.
Read through [`package.json`](/configuring-npm/package-json) to see all the things that you can specify and enable by simply describing your package appropriately.
In general, this will lead to a more robust and consistent state.
* Inspect the env to determine where to put things.
For instance, if the `NPM_CONFIG_BINROOT` environment variable is set to `/home/user/bin`, then don't try to install executables into `/usr/local/bin`.
The user probably set it up that way for a reason.
* Don't prefix your script commands with "sudo".  If root permissions are required for some reason, then it'll fail with that error, and the user will sudo the npm command in question.
* Don't use `install`.
Use a `.gyp` file for compilation, and `prepare` for anything else.
You should almost never have to explicitly set a preinstall or install script.
If you are doing this, please consider if there is another option.
The only valid use of `install` or `preinstall` scripts is for compilation which must be done on the target architecture.

### See Also

* [npm run](/commands/npm-run)
* [package.json](/configuring-npm/package-json)
* [npm developers](/using-npm/developers)
* [npm install](/commands/npm-install)
