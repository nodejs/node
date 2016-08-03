# Debugging Guide

<!-- type=misc -->

The goal of this guide is to provide you with enough information to get started
debugging your Node.js apps and scripts.

## Enable Debugging

When started with the **--debug** or **--debug-brk** switches, Node.js listens
for debugging commands defined by the [V8 Debugging Protocol][] on a TCP port,
by default `5858`. Any debugger client which speaks this protocol can connect
to and debug the running process.

A running Node.js process can also be instructed to start listening for
debugging messages by signaling it with `SIGUSR1` (on Linux and OS X).

When started with the **--inspect** or **--inspect-brk** switches, Node.js
listens for diagnostic commands defined by the [Chrome Debugging Protocol][]
via websockets, by default at `ws://127.0.0.1:9229/node`. Any diagnostics
client which speaks this protocol can connect to and debug the running process.

The `--inspect` option and protocol are _experimental_ and may change.

[V8 Debugging Protocol]: https://github.com/v8/v8/wiki/Debugging-Protocol
[Chrome Debugging Protocol]: https://developer.chrome.com/devtools/docs/debugger-protocol


## Debugging Clients

Several commercial and open source tools can connect to Node's
debugger and/or inspector. Info on these follows.

* [Built-in Debugger](https://github.com/nodejs/node/blob/master/lib/_debugger.js)

  * Start `node debug script_name.js` to start your script under Node's
    builtin command-line debugger. Your script starts in another Node
    process started with the `--debug-brk` option, and the initial Node
    process runs the `_debugger.js` script and connects to your target.

* [Chrome DevTools](https://github.com/ChromeDevTools/devtools-frontend)

  * **Option 1 (Chrome 55+)**: Open `chrome://inspect` in your Chromium
    browser. Click the Configure button and ensure your target host and port
    are listed.
  * **Option 2**: Paste the following URL in Chrome,
    replacing the `ws` parameter with your hostname and port as needed:

    `chrome-devtools://devtools/remote/serve_file/@60cd6e859b9f557d2312f5bf532f6aec5f284980/inspector.html?experiments=true&v8only=true&ws=localhost:9229/node`
    
    > The hash represents the current revision and is specified in
      <https://github.com/nodejs/node/blob/master/deps/v8_inspector/third_party/v8_inspector/platform/v8_inspector/public/InspectorVersion.h>

* [VS Code](https://github.com/microsoft/vscode)
  * **Option 1**: In the Debug panel, click the settings button to choose
    initial debug configurations. Select "Node.js v6.3+ (Experimental)" for
    `--inspect` support, or "Node.js" for `--debug` support.
  * **Option 2**: In debug configurations in `.vscode/launch.json`, set `type`
    as `node2`.

  * For more info, see <https://github.com/Microsoft/vscode-node-debug2>.

* [IntelliJ WebStorm](tbd)

* [node-inspector](https://github.com/node-inspector/node-inspector)

* [chrome-remote-interface](https://github.com/cyrus-and/chrome-remote-interface)


## Debugging command-line options

* The following table clarifies the implications of various diag-related flags:

<table cellpadding=0 cellspacing=0>
  <tr><th>Flag</th><th>Meaning</th></tr>
  <tr>
    <td>--debug</td>
    <td>
      <ul>
        <li>Enable debugger agent</li>
        <li>Listen on default port (5858)</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--debug=<i>port</i></td>
    <td>
      <ul>
        <li>Enable debugger agent</li>
        <li>Listen on port <i>port</i></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--debug-brk</td>
    <td>
      <ul>
        <li>Enable debugger agent</li>
        <li>Listen on default port (5858)</li>
        <li>Break before user code starts</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--debug-brk=<i>port</i></td>
    <td>
      <ul>
        <li>Enable debugger agent</li>
        <li>Listen on port <i>port</i></li>
        <li>Break before user code starts</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--inspect</td>
    <td>
      <ul>
        <li>Enable inspector agent</li>
        <li>Listen on default port (9229)</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--inspect=<i>port</i></td>
    <td>
      <ul>
        <li>Enable inspector agent</li>
        <li>Listen on port <i>port</i></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--inspect-brk</td>
    <td>
      <ul>
        <li>Enable inspector agent</li>
        <li>Listen on default port (9229)</li>
        <li>Break before user code starts</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--inspect-brk=<i>port</i></td>
    <td>
      <ul>
        <li>Enable debugger agent</li>
        <li>Listen on port <i>port</i></li>
        <li>Break before user code starts</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>--debug-port=<i>port</i></td>
    <td>
      <ul>
        <li> Set port to <i>port</i> for other flags. Must come after others.
      </ul>
    </td>
  </tr>
  <tr>
    <td><code>node debug <i>script.js</i></code></td>
    <td>
      <ul>
        <li>Spawn child process to run user script under --debug mode,
            and use main process to run CLI debugger.</li>
      </ul>
    </td>
  </tr>
</table>


## Default ports: 

* Inpsector: 9229
* Debugger:  5858

