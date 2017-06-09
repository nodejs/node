// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest = {};
InspectorTest._dispatchTable = new Map();
InspectorTest._requestId = 0;
InspectorTest._dumpInspectorProtocolMessages = false;
InspectorTest._eventHandler = {};
InspectorTest._commandsForLogging = new Set();

Protocol = new Proxy({}, {
  get: function(target, agentName, receiver) {
    return new Proxy({}, {
      get: function(target, methodName, receiver) {
        const eventPattern = /^on(ce)?([A-Z][A-Za-z0-9]+)/;
        var match = eventPattern.exec(methodName);
        if (!match) {
          return (args, contextGroupId) => InspectorTest._sendCommandPromise(`${agentName}.${methodName}`, args || {}, contextGroupId);
        } else {
          var eventName = match[2];
          eventName = eventName.charAt(0).toLowerCase() + eventName.slice(1);
          if (match[1])
            return () => InspectorTest._waitForEventPromise(
                       `${agentName}.${eventName}`);
          else
            return (listener) => { InspectorTest._eventHandler[`${agentName}.${eventName}`] = listener };
        }
      }
    });
  }
});

InspectorTest.logProtocolCommandCalls = (command) => InspectorTest._commandsForLogging.add(command);

InspectorTest.log = utils.print.bind(null);

InspectorTest.logMessage = function(originalMessage)
{
  var message = JSON.parse(JSON.stringify(originalMessage));
  if (message.id)
    message.id = "<messageId>";

  const nonStableFields = new Set(["objectId", "scriptId", "exceptionId", "timestamp",
    "executionContextId", "callFrameId", "breakpointId", "bindRemoteObjectFunctionId", "formatterObjectId" ]);
  var objects = [ message ];
  while (objects.length) {
    var object = objects.shift();
    for (var key in object) {
      if (nonStableFields.has(key))
        object[key] = `<${key}>`;
      else if (typeof object[key] === "string" && object[key].match(/\d+:\d+:\d+:debug/))
        object[key] = object[key].replace(/\d+/, '<scriptId>');
      else if (typeof object[key] === "object")
        objects.push(object[key]);
    }
  }

  InspectorTest.logObject(message);
  return originalMessage;
}

InspectorTest.logObject = function(object, title)
{
  var lines = [];

  function dumpValue(value, prefix, prefixWithName)
  {
    if (typeof value === "object" && value !== null) {
      if (value instanceof Array)
        dumpItems(value, prefix, prefixWithName);
      else
        dumpProperties(value, prefix, prefixWithName);
    } else {
      lines.push(prefixWithName + String(value).replace(/\n/g, " "));
    }
  }

  function dumpProperties(object, prefix, firstLinePrefix)
  {
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    lines.push(firstLinePrefix + "{");

    var propertyNames = Object.keys(object);
    propertyNames.sort();
    for (var i = 0; i < propertyNames.length; ++i) {
      var name = propertyNames[i];
      if (!object.hasOwnProperty(name))
        continue;
      var prefixWithName = "    " + prefix + name + " : ";
      dumpValue(object[name], "    " + prefix, prefixWithName);
    }
    lines.push(prefix + "}");
  }

  function dumpItems(object, prefix, firstLinePrefix)
  {
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    lines.push(firstLinePrefix + "[");
    for (var i = 0; i < object.length; ++i)
      dumpValue(object[i], "    " + prefix, "    " + prefix + "[" + i + "] : ");
    lines.push(prefix + "]");
  }

  dumpValue(object, "", title || "");
  InspectorTest.log(lines.join("\n"));
}

InspectorTest.logCallFrames = function(callFrames)
{
  for (var frame of callFrames) {
    var functionName = frame.functionName || '(anonymous)';
    var url = frame.url ? frame.url : InspectorTest._scriptMap.get(frame.location.scriptId).url;
    var lineNumber = frame.location ? frame.location.lineNumber : frame.lineNumber;
    var columnNumber = frame.location ? frame.location.columnNumber : frame.columnNumber;
    InspectorTest.log(`${functionName} (${url}:${lineNumber}:${columnNumber})`);
  }
}

InspectorTest.logSourceLocation = function(location)
{
  var scriptId = location.scriptId;
  if (!InspectorTest._scriptMap || !InspectorTest._scriptMap.has(scriptId)) {
    InspectorTest.log("InspectorTest.setupScriptMap should be called before Protocol.Debugger.enable.");
    InspectorTest.completeTest();
  }
  var script = InspectorTest._scriptMap.get(scriptId);
  if (!script.scriptSource) {
    // TODO(kozyatinskiy): doesn't assume that contextId == contextGroupId.
    return Protocol.Debugger.getScriptSource({ scriptId }, script.executionContextId)
      .then(message => script.scriptSource = message.result.scriptSource)
      .then(dumpSourceWithLocation);
  }
  return Promise.resolve().then(dumpSourceWithLocation);

  function dumpSourceWithLocation() {
    var lines = script.scriptSource.split('\n');
    var line = lines[location.lineNumber];
    line = line.slice(0, location.columnNumber) + '#' + (line.slice(location.columnNumber) || '');
    lines[location.lineNumber] = line;
    lines = lines.filter(line => line.indexOf('//# sourceURL=') === -1);
    InspectorTest.log(lines.slice(Math.max(location.lineNumber - 1, 0), location.lineNumber + 2).join('\n'));
    InspectorTest.log('');
  }
}

InspectorTest.logSourceLocations = function(locations) {
  if (locations.length == 0) return Promise.resolve();
  return InspectorTest.logSourceLocation(locations[0])
      .then(() => InspectorTest.logSourceLocations(locations.splice(1)));
}

InspectorTest.logAsyncStackTrace = function(asyncStackTrace)
{
  while (asyncStackTrace) {
    if (asyncStackTrace.promiseCreationFrame) {
      var frame = asyncStackTrace.promiseCreationFrame;
      InspectorTest.log(`-- ${asyncStackTrace.description} (${frame.url
                        }:${frame.lineNumber}:${frame.columnNumber})--`);
    } else {
      InspectorTest.log(`-- ${asyncStackTrace.description} --`);
    }
    InspectorTest.logCallFrames(asyncStackTrace.callFrames);
    asyncStackTrace = asyncStackTrace.parent;
  }
}

InspectorTest.completeTest = () => Protocol.Debugger.disable().then(() => utils.quit());

InspectorTest.completeTestAfterPendingTimeouts = function()
{
  InspectorTest.waitPendingTasks().then(InspectorTest.completeTest);
}

InspectorTest.waitPendingTasks = function()
{
  return Protocol.Runtime.evaluate({ expression: "new Promise(r => setTimeout(r, 0))//# sourceURL=wait-pending-tasks.js", awaitPromise: true });
}

InspectorTest.addScript = (string, lineOffset, columnOffset) => utils.compileAndRunWithOrigin(string, "", lineOffset || 0, columnOffset || 0, false);
InspectorTest.addScriptWithUrl = (string, url) => utils.compileAndRunWithOrigin(string, url, 0, 0, false);
InspectorTest.addModule = (string, url, lineOffset, columnOffset) => utils.compileAndRunWithOrigin(string, url, lineOffset || 0, columnOffset || 0, true);

InspectorTest.startDumpingProtocolMessages = function()
{
  InspectorTest._dumpInspectorProtocolMessages = true;
}

InspectorTest.sendRawCommand = function(requestId, command, handler, contextGroupId)
{
  if (InspectorTest._dumpInspectorProtocolMessages)
    utils.print("frontend: " + command);
  InspectorTest._dispatchTable.set(requestId, handler);
  sendMessageToBackend(command, contextGroupId || 0);
}

InspectorTest.checkExpectation = function(fail, name, messageObject)
{
  if (fail === !!messageObject.error) {
    InspectorTest.log("PASS: " + name);
    return true;
  }

  InspectorTest.log("FAIL: " + name + ": " + JSON.stringify(messageObject));
  InspectorTest.completeTest();
  return false;
}
InspectorTest.expectedSuccess = InspectorTest.checkExpectation.bind(null, false);
InspectorTest.expectedError = InspectorTest.checkExpectation.bind(null, true);

InspectorTest.setupScriptMap = function() {
  if (InspectorTest._scriptMap)
    return;
  InspectorTest._scriptMap = new Map();
}

InspectorTest.runTestSuite = function(testSuite)
{
  function nextTest()
  {
    if (!testSuite.length) {
      InspectorTest.completeTest();
      return;
    }
    var fun = testSuite.shift();
    InspectorTest.log("\nRunning test: " + fun.name);
    fun(nextTest);
  }
  nextTest();
}

InspectorTest.runAsyncTestSuite = async function(testSuite) {
  for (var test of testSuite) {
    InspectorTest.log("\nRunning test: " + test.name);
    await test();
  }
  InspectorTest.completeTest();
}

InspectorTest._sendCommandPromise = function(method, params, contextGroupId)
{
  var requestId = ++InspectorTest._requestId;
  var messageObject = { "id": requestId, "method": method, "params": params };
  var fulfillCallback;
  var promise = new Promise(fulfill => fulfillCallback = fulfill);
  if (InspectorTest._commandsForLogging.has(method)) {
    utils.print(method + ' called');
  }
  InspectorTest.sendRawCommand(requestId, JSON.stringify(messageObject), fulfillCallback, contextGroupId);
  return promise;
}

InspectorTest._waitForEventPromise = function(eventName)
{
  return new Promise(fulfill => InspectorTest._eventHandler[eventName] = fullfillAndClearListener.bind(null, fulfill));

  function fullfillAndClearListener(fulfill, result)
  {
    delete InspectorTest._eventHandler[eventName];
    fulfill(result);
  }
}

InspectorTest._dispatchMessage = function(messageObject)
{
  if (InspectorTest._dumpInspectorProtocolMessages)
    utils.print("backend: " + JSON.stringify(messageObject));
  try {
    var messageId = messageObject["id"];
    if (typeof messageId === "number") {
      var handler = InspectorTest._dispatchTable.get(messageId);
      if (handler) {
        handler(messageObject);
        InspectorTest._dispatchTable.delete(messageId);
      }
    } else {
      var eventName = messageObject["method"];
      var eventHandler = InspectorTest._eventHandler[eventName];
      if (InspectorTest._scriptMap && eventName === "Debugger.scriptParsed")
        InspectorTest._scriptMap.set(messageObject.params.scriptId, JSON.parse(JSON.stringify(messageObject.params)));
      if (eventName === "Debugger.scriptParsed" && messageObject.params.url === "wait-pending-tasks.js")
        return;
      if (eventHandler)
        eventHandler(messageObject);
    }
  } catch (e) {
    InspectorTest.log("Exception when dispatching message: " + e + "\n" + e.stack + "\n message = " + JSON.stringify(messageObject, null, 2));
    InspectorTest.completeTest();
  }
}

InspectorTest.loadScript = function(fileName) {
  InspectorTest.addScript(utils.read(fileName));
}

InspectorTest.setupInjectedScriptEnvironment = function(debug) {
  let scriptSource = '';
  // First define all getters on Object.prototype.
  let injectedScriptSource = utils.read('src/inspector/injected-script-source.js');
  let getterRegex = /\.[a-zA-Z0-9]+/g;
  let match;
  let getters = new Set();
  while (match = getterRegex.exec(injectedScriptSource)) {
    getters.add(match[0].substr(1));
  }
  scriptSource += `(function installSettersAndGetters() {
    let defineProperty = Object.defineProperty;
    let ObjectPrototype = Object.prototype;\n`;
  scriptSource += Array.from(getters).map(getter => `
    defineProperty(ObjectPrototype, '${getter}', {
      set() { debugger; throw 42; }, get() { debugger; throw 42; },
      __proto__: null
    });
  `).join('\n') + '})();';
  InspectorTest.addScript(scriptSource);

  if (debug) {
    InspectorTest.log('WARNING: InspectorTest.setupInjectedScriptEnvironment with debug flag for debugging only and should not be landed.');
    InspectorTest.log('WARNING: run test with --expose-inspector-scripts flag to get more details.');
    InspectorTest.log('WARNING: you can additionally comment rjsmin in xxd.py to get unminified injected-script-source.js.');
    InspectorTest.setupScriptMap();
    Protocol.Debugger.enable();
    Protocol.Debugger.onPaused(message => {
      let callFrames = message.params.callFrames;
      InspectorTest.logSourceLocations(callFrames.map(frame => frame.location));
    })
  }
}
