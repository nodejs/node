// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This helper allows to debug d8 using Chrome DevTools.
//
// It runs a simple REPL for inspector messages and relies on
// websocketd (https://github.com/joewalnes/websocketd) for the WebSocket
// communication.
//
// You can start a session with a debug build of d8 like:
//
// $ websocketd out/x64.debug/d8 YOUR_SCRIPT.js tools/inspect-d8.js
//
// After that, copy the URL from console and pass it as `ws=` parameter to
// the Chrome DevTools frontend like:
//
// chrome-devtools://devtools/bundled/js_app.html?ws=localhost:80

function receive(msg) {
  print(msg);
}

function handleInspectorMessage() {
  send(readline());
}

while (true) {
  handleInspectorMessage();
}
