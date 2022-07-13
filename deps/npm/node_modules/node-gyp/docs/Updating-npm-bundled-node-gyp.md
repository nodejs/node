# Updating the npm-bundled version of node-gyp

**Note: These instructions are (only) tested and known to work with npm 8 and older.**

**Note: These instructions will be undone if you reinstall or upgrade npm or node! For a more permanent (and simpler) solution, see [Force-npm-to-use-global-node-gyp.md](Force-npm-to-use-global-node-gyp.md). (npm 6 or older only!)**

[Many issues](https://github.com/nodejs/node-gyp/labels/ERR%21%20node-gyp%20-v%20%3C%3D%20v5.1.0) are opened by users who are
not running a [current version of node-gyp](https://github.com/nodejs/node-gyp/releases).

`npm` bundles its own, internal, copy of `node-gyp`. This internal copy is independent of any globally installed copy of node-gyp that
may have been installed via `npm install -g node-gyp`.

This means that while `node-gyp` doesn't get installed into your `$PATH` by default, npm still keeps its own copy to invoke when you
attempt to `npm install` a native add-on.

Sometimes, you may need to update npm's internal node-gyp to a newer version than what is installed. A simple `npm install -g node-gyp`
_won't_ do the trick since npm will still continue to use its internal copy over the global one.

So instead:

## Version of npm

We need to start by knowing your version of `npm`:
```bash
npm --version
```

## Linux, macOS, Solaris, etc.

Unix is easy. Just run the following command.

If your npm is version ___7 or 8___, do:
```bash
$ npm explore npm/node_modules/@npmcli/run-script -g -- npm_config_global=false npm install node-gyp@latest
```

Else if your npm is version ___less than 7___, do:
```bash
$ npm explore npm/node_modules/npm-lifecycle -g -- npm install node-gyp@latest
```

If the command fails with a permissions error, please try `sudo` and then the command.

## Windows

Windows is a bit trickier, since `npm` might be installed to the "Program Files" directory, which needs admin privileges in order to
modify on current Windows. Therefore, run the following commands __inside a `cmd.exe` started with "Run as Administrator"__:

First we need to find the location of `node`. If you don't already know the location that `node.exe` got installed to, then run:
```bash
$ where node
```

Now `cd` to the directory that `node.exe` is contained in e.g.:
```bash
$ cd "C:\Program Files\nodejs"
```

If your npm version is ___7 or 8___, do:
```bash
cd node_modules\npm\node_modules\@npmcli\run-script
```

Else if your npm version is ___less than 7___, do:
```bash
cd node_modules\npm\node_modules\npm-lifecycle
```

Finish by running:
```bash
$ npm install node-gyp@latest
```
