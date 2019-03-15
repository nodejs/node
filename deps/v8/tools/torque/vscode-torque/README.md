# Torque support

This extension adds language support for [the Torque language used in V8](https://v8.dev/docs/torque).

## Installation

Since the extension is currently not published to the marketplace, the easiest way to
install the extension is to symlink it to your local extension directory:

```
ln -s $V8/tools/torque/vscode-torque $HOME/.vscode/extensions/vscode-torque
```

Additionally, for advanced language server features, the extension needs to be built
locally (the syntax highlighting does not require this step). The following needs to be run
everytime the extension is updated:

```
cd $V8/tools/torque/vscode-torque
npm install
```

### Language server

The language server is not built by default. To build the language server manually:

```
autoninja -C <output dir> torque-language-server
```

The default directory where the extension looks for the executable is "out/x64.release",
but the absolute path to the executable can be configured with the `torque.ls.executable`
setting.
