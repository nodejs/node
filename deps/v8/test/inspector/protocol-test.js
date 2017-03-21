// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest = {};
InspectorTest._dispatchTable = new Map();
InspectorTest._requestId = 0;
InspectorTest._dumpInspectorProtocolMessages = false;
InspectorTest._eventHandler = {};

Protocol = new Proxy({}, {
  get: function(target, agentName, receiver) {
    return new Proxy({}, {
      get: function(target, methodName, receiver) {
        const eventPattern = /^on(ce)?([A-Z][A-Za-z0-9]+)/;
        var match = eventPattern.exec(methodName);
        if (!match) {
          return (args) => InspectorTest._sendCommandPromise(`${agentName}.${methodName}`, args || {});
        } else {
          var eventName = match[2];
          eventName = eventName.charAt(0).toLowerCase() + eventName.slice(1);
          if (match[1])
            return (args) => InspectorTest._waitForEventPromise(`${agentName}.${eventName}`, args || {});
          else
            return (listener) => { InspectorTest._eventHandler[`${agentName}.${eventName}`] = listener };
        }
      }
    });
  }
});

InspectorTest.log = print.bind(null);

InspectorTest.logMessage = function(originalMessage)
{
  var message = JSON.parse(JSON.stringify(originalMessage));
  if (message.id)
    message.id = "<messageId>";

  const nonStableFields = new Set(["objectId", "scriptId", "exceptionId", "timestamp", "executionContextId", "callFrameId", "breakpointId"]);
  var objects = [ message ];
  while (objects.length) {
    var object = objects.shift();
    for (var key in object) {
      if (nonStableFields.has(key))
        object[key] = `<${key}>`;
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

InspectorTest.completeTest = function()
{
  Protocol.Debugger.disable().then(() => quit());
}

InspectorTest.completeTestAfterPendingTimeouts = function()
{
  Protocol.Runtime.evaluate({
    expression: "new Promise(resolve => setTimeout(resolve, 0))",
    awaitPromise: true }).then(InspectorTest.completeTest);
}

InspectorTest.addScript = (string, lineOffset, columnOffset) => compileAndRunWithOrigin(string, "", lineOffset || 0, columnOffset || 0);
InspectorTest.addScriptWithUrl = (string, url) => compileAndRunWithOrigin(string, url, 0, 0);

InspectorTest.startDumpingProtocolMessages = function()
{
  InspectorTest._dumpInspectorProtocolMessages = true;
}

InspectorTest.sendRawCommand = function(requestId, command, handler)
{
  if (InspectorTest._dumpInspectorProtocolMessages)
    print("frontend: " + command);
  InspectorTest._dispatchTable.set(requestId, handler);
  sendMessageToBackend(command);
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

InspectorTest._sendCommandPromise = function(method, params)
{
  var requestId = ++InspectorTest._requestId;
  var messageObject = { "id": requestId, "method": method, "params": params };
  var fulfillCallback;
  var promise = new Promise(fulfill => fulfillCallback = fulfill);
  InspectorTest.sendRawCommand(requestId, JSON.stringify(messageObject), fulfillCallback);
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
    print("backend: " + JSON.stringify(messageObject));
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
      if (eventHandler)
        eventHandler(messageObject);
    }
  } catch (e) {
    InspectorTest.log("Exception when dispatching message: " + e + "\n" + e.stack + "\n message = " + JSON.stringify(messageObject, null, 2));
    InspectorTest.completeTest();
  }
}
