// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Debugger.getPossibleBreakpoints');

var source = utils.read('test/inspector/debugger/resources/break-locations.js');
contextGroup.addScript(source);

Protocol.Debugger.onceScriptParsed()
  .then(message => Protocol.Debugger.getPossibleBreakpoints({ start: { lineNumber: 0, columnNumber : 0, scriptId: message.params.scriptId }}))
  .then(dumpAllLocations)
  .then(InspectorTest.completeTest);
Protocol.Debugger.enable();

function dumpAllLocations(message) {
  if (message.error) {
    InspectorTest.logMessage(message);
    return;
  }
  var lines = source.split('\n');
  var locations = message.result.locations.sort((loc1, loc2) => {
    if (loc2.lineNumber !== loc1.lineNumber) return loc2.lineNumber - loc1.lineNumber;
    return loc2.columnNumber - loc1.columnNumber;
  });
  for (var location of locations) {
    var line = lines[location.lineNumber];
    line = line.slice(0, location.columnNumber) + locationMark(location.type) + line.slice(location.columnNumber);
    lines[location.lineNumber] = line;
  }
  lines = lines.filter(line => line.indexOf('//# sourceURL=') === -1);
  InspectorTest.log(lines.join('\n'));
  return message;
}

function locationMark(type) {
  if (type === 'return') return '|R|';
  if (type === 'call') return '|C|';
  if (type === 'debuggerStatement') return '|D|';
  return '|_|';
}
