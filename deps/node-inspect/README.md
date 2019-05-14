# `node-inspect`

```bash
npm install --global node-inspect
```

For the old V8 debugger protocol,
node has two options:

1. `node --debug <file>`: Start `file` with remote debugging enabled.
2. `node debug <file>`: Start an interactive CLI debugger for `<file>`.

But for the Chrome inspector protocol,
there's only one: `node --inspect <file>`.

This project tries to provide the missing second option
by re-implementing `node debug` against the new protocol.

```
Usage: node-inspect script.js
       node-inspect <host>:<port>
```

#### References

* [Debugger Documentation](https://nodejs.org/api/debugger.html)
* [EPS: `node inspect` CLI debugger](https://github.com/nodejs/node-eps/pull/42)
* [Debugger Protocol Viewer](https://chromedevtools.github.io/debugger-protocol-viewer/)
* [Command Line API](https://developers.google.com/web/tools/chrome-devtools/debug/command-line/command-line-reference?hl=en)
