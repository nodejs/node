# node-pty

[![Build Status](https://dev.azure.com/vscode/node-pty/_apis/build/status/Microsoft.node-pty)](https://dev.azure.com/vscode/node-pty/_build/latest?definitionId=11)

`forkpty(3)` bindings for node.js. This allows you to fork processes with pseudoterminal file descriptors. It returns a terminal object which allows reads and writes.

This is useful for:

- Writing a terminal emulator (eg. via [xterm.js](https://github.com/sourcelair/xterm.js)).
- Getting certain programs to *think* you're a terminal, such as when you need a program to send you control sequences.

`node-pty` supports Linux, macOS and Windows. Windows support is possible by utilizing the [Windows conpty API](https://blogs.msdn.microsoft.com/commandline/2018/08/02/windows-command-line-introducing-the-windows-pseudo-console-conpty/) on Windows 1809+ and the [winpty](https://github.com/rprichard/winpty) library in older version.

## API

The full API for node-pty is contained within the [TypeScript declaration file](https://github.com/microsoft/node-pty/blob/master/typings/node-pty.d.ts), use the branch/tag picker in GitHub (`w`) to navigate to the correct version of the API.

## Example Usage

```js
var os = require('os');
var pty = require('node-pty');

var shell = os.platform() === 'win32' ? 'powershell.exe' : 'bash';

var ptyProcess = pty.spawn(shell, [], {
  name: 'xterm-color',
  cols: 80,
  rows: 30,
  cwd: process.env.HOME,
  env: process.env
});

ptyProcess.on('data', function(data) {
  process.stdout.write(data);
});

ptyProcess.write('ls\r');
ptyProcess.resize(100, 40);
ptyProcess.write('ls\r');
```

## Real-world Uses

`node-pty` powers many different terminal emulators, including:

- [Microsoft Visual Studio Code](https://code.visualstudio.com)
- [Hyper](https://hyper.is/)
- [Upterm](https://github.com/railsware/upterm)
- [Script Runner](https://github.com/ioquatix/script-runner) for Atom.
- [Theia](https://github.com/theia-ide/theia)
- [FreeMAN](https://github.com/matthew-matvei/freeman) file manager
- [terminus](https://atom.io/packages/terminus) - An Atom plugin for providing terminals inside your Atom workspace.
- [x-terminal](https://atom.io/packages/x-terminal) - Also an Atom plugin that provides terminals inside your Atom workspace.
- [Termination](https://atom.io/packages/termination) - Also an Atom plugin that provides terminals inside your Atom workspace.
- [atom-xterm](https://atom.io/packages/atom-xterm) - Also an Atom plugin that provides terminals inside your Atom workspace.
- [electerm](https://github.com/electerm/electerm) Terminal/SSH/SFTP client(Linux, macOS, Windows).
- [Extraterm](http://extraterm.org/)
- [Wetty](https://github.com/krishnasrinivas/wetty) Browser based Terminal over HTTP and HTTPS
- [nomad](https://github.com/lukebarnard1/nomad-term)
- [DockerStacks](https://github.com/sfx101/docker-stacks) Local LAMP/LEMP stack using Docker
- [TeleType](https://github.com/akshaykmr/TeleType): cli tool that allows you to share your terminal online conveniently. Show off mad cli-fu, help a colleague, teach, or troubleshoot.
- [mesos-term](https://github.com/criteo/mesos-term): A web terminal for Apache Mesos. It allows to execute commands within containers.
- [Commas](https://github.com/CyanSalt/commas): A hackable terminal and command runner.
- [ENiGMA½ BBS Software](https://github.com/NuSkooler/enigma-bbs): A modern BBS software with a nostalgic flair!

Do you use node-pty in your application as well? Please open a [Pull Request](https://github.com/Tyriar/node-pty/pulls) to include it here. We would love to have it in our list.

## Building

```bash
# Install dependencies and build C++
npm install
# Compile TypeScript -> JavaScript
npm run build
```

## Dependencies

### Linux/Ubuntu

```
sudo apt install -y make python build-essential
```

The following are also needed:

- Node.JS 10+

### macOS

Xcode is needed to compile the sources, this can be installed from the App Store.

### Windows

`npm install` requires some tools to be present in the system like Python and C++ compiler. Windows users can easily install them by running the following command in PowerShell as administrator. For more information see https://github.com/felixrieseberg/windows-build-tools:

```sh
npm install --global --production windows-build-tools
```

The following are also needed:

- [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk) - only the "Desktop C++ Apps" components are needed to be installed
- Node.JS 10+

## Debugging

[The wiki](https://github.com/Microsoft/node-pty/wiki/Debugging) contains instructions for debugging node-pty.

## Security

All processes launched from node-pty will launch at the same permission level of the parent process. Take care particularly when using node-pty inside a server that's accessible on the internet. We recommend launching the pty inside a container to protect your host machine.

## Thread Safety

Note that node-pty is not thread safe so running it across multiple worker threads in node.js could cause issues.

## Flow Control

Automatic flow control can be enabled by either providing `handleFlowControl = true` in the constructor options or setting it later on:

```js
const PAUSE = '\x13';   // XOFF
const RESUME = '\x11';  // XON

const ptyProcess = pty.spawn(shell, [], {handleFlowControl: true});

// flow control in action
ptyProcess.write(PAUSE);  // pty will block and pause the child program
...
ptyProcess.write(RESUME); // pty will enter flow mode and resume the child program

// temporarily disable/re-enable flow control
ptyProcess.handleFlowControl = false;
...
ptyProcess.handleFlowControl = true;
```

By default `PAUSE` and `RESUME` are XON/XOFF control codes (as shown above). To avoid conflicts in environments that use these control codes for different purposes the messages can be customized as `flowControlPause: string` and `flowControlResume: string` in the constructor options. `PAUSE` and `RESUME` are not passed to the underlying pseudoterminal if flow control is enabled.

## Troubleshooting

### Powershell gives error 8009001d

> Internal Windows PowerShell error.  Loading managed Windows PowerShell failed with error 8009001d.

This happens when PowerShell is launched with no `SystemRoot` environment variable present.

### ConnectNamedPipe failed: Windows error 232

This error can occur due to anti-virus software intercepting winpty from creating a pty. To workaround this you can exclude this file from your anti-virus scanning `node-pty\build\Release\winpty-agent.exe`

## pty.js

This project is forked from [chjj/pty.js](https://github.com/chjj/pty.js) with the primary goals being to provide better support for later Node.JS versions and Windows.

## License

Copyright (c) 2012-2015, Christopher Jeffrey (MIT License).<br>
Copyright (c) 2016, Daniel Imms (MIT License).<br>
Copyright (c) 2018, Microsoft Corporation (MIT License).
