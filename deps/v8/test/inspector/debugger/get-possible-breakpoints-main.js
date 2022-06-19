// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Debugger.getPossibleBreakpoints');

var source = utils.read('test/inspector/debugger/resources/break-locations.js');
contextGroup.addScript(source);
session.setupScriptMap();

Protocol.Debugger.onceScriptParsed()
  .then(message => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber : 0, scriptId: message.params.scriptId }}))
  .then(message => session.logBreakLocations(message.result.locations))
  .then(InspectorTest.completeTest);
Protocol.Debugger.enable();
