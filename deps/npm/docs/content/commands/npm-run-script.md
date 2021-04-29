---
title: npm-run-script
section: 1
description: Run arbitrary package scripts
---

### Synopsis

```bash
npm run-script <command> [--if-present] [--silent] [-- <args>]
npm run-script <command> [--workspace=<workspace-name>]
npm run-script <command> [--workspaces]

aliases: run, rum, urn
```

### Description

This runs an arbitrary command from a package's `"scripts"` object.  If no
`"command"` is provided, it will list the available scripts.

`run[-script]` is used by the test, start, restart, and stop commands, but
can be called directly, as well. When the scripts in the package are
printed out, they're separated into lifecycle (test, start, restart) and
directly-run scripts.

Any positional arguments are passed to the specified script.  Use `--` to
pass `-`-prefixed flags and options which would otherwise be parsed by npm.

For example:

```bash
npm run test -- --grep="pattern"
```

The arguments will only be passed to the script specified after ```npm run```
and not to any `pre` or `post` script.

The `env` script is a special built-in command that can be used to list
environment variables that will be available to the script at runtime. If an
"env" command is defined in your package, it will take precedence over the
built-in.

In addition to the shell's pre-existing `PATH`, `npm run` adds
`node_modules/.bin` to the `PATH` provided to scripts. Any binaries
provided by locally-installed dependencies can be used without the
`node_modules/.bin` prefix. For example, if there is a `devDependency` on
`tap` in your package, you should write:

```bash
"scripts": {"test": "tap test/*.js"}
```

instead of

```bash
"scripts": {"test": "node_modules/.bin/tap test/*.js"}
```

The actual shell your script is run within is platform dependent. By default,
on Unix-like systems it is the `/bin/sh` command, on Windows it is
`cmd.exe`.
The actual shell referred to by `/bin/sh` also depends on the system.
You can customize the shell with the `script-shell` configuration.

Scripts are run from the root of the package folder, regardless of what the
current working directory is when `npm run` is called. If you want your
script to use different behavior based on what subdirectory you're in, you
can use the `INIT_CWD` environment variable, which holds the full path you
were in when you ran `npm run`.

`npm run` sets the `NODE` environment variable to the `node` executable
with which `npm` is executed. Also, if the `--scripts-prepend-node-path` is
passed, the directory within which `node` resides is added to the `PATH`.
If `--scripts-prepend-node-path=auto` is passed (which has been the default
in `npm` v3), this is only performed when that `node` executable is not
found in the `PATH`.

If you try to run a script without having a `node_modules` directory and it
fails, you will be given a warning to run `npm install`, just in case you've
forgotten.

### Workspaces support

You may use the `workspace` or `workspaces` configs in order to run an
arbitrary command from a package's `"scripts"` object in the context of the
specified workspaces. If no `"command"` is provided, it will list the available
scripts for each of these configured workspaces.

Given a project with configured workspaces, e.g:

```
.
+-- package.json
`-- packages
   +-- a
   |   `-- package.json
   +-- b
   |   `-- package.json
   `-- c
       `-- package.json
```

Assuming the workspace configuration is properly set up at the root level
`package.json` file. e.g:

```
{
    "workspaces": [ "./packages/*" ]
}
```

And that each of the configured workspaces has a configured `test` script,
we can run tests in all of them using the `workspaces` config:

```
npm test --workspaces
```

#### Filtering workspaces

It's also possible to run a script in a single workspace using the `workspace`
config along with a name or directory path:

```
npm test --workspace=a
```

The `workspace` config can also be specified multiple times in order to run a
specific script in the context of multiple workspaces. When defining values for
the `workspace` config in the command line, it also possible to use `-w` as a
shorthand, e.g:

```
npm test -w a -w b
```

This last command will run `test` in both `./packages/a` and `./packages/b`
packages.


### Configuration

#### if-present

* Type: Boolean
* Default: false

You can use the `--if-present` flag to avoid exiting with a non-zero exit code
when the script is undefined. This lets you run potentially undefined scripts
without breaking the execution chain.

#### ignore-scripts

* Type: Boolean
* Default: false

Skips running `pre` and `post` scripts.

#### script-shell

* Type: String
* Default: `null`

Optional custom script to use to execute the command. If not defined defaults
to `/bin/sh` on Unix, defaults to `env.comspec` or `cmd.exe` on Windows.

#### silent

* Type: Boolean
* Default: false

You can use the `--silent` flag to prevent showing `npm ERR!` output on error.

#### workspace

* Alias: `-w`
* Type: Array
* Default: `[]`

Enable running scripts in the context of workspaces while also filtering by
the provided names or paths provided.

Valid values for the `workspace` config are either:
- Workspace names
- Path to a workspace directory
- Path to a parent workspace directory (will result to selecting all of the
children workspaces)

#### workspaces

* Alias: `--ws`
* Type: Boolean
* Default: `false`

Run scripts in the context of all configured workspaces for the current
project.

### See Also

* [npm scripts](/using-npm/scripts)
* [npm test](/commands/npm-test)
* [npm start](/commands/npm-start)
* [npm restart](/commands/npm-restart)
* [npm stop](/commands/npm-stop)
* [npm config](/commands/npm-config)
* [npm workspaces](/using-npm/workspaces)
