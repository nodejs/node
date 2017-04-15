// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A general-purpose engine for sending a sequence of protocol commands.
// The clients provide requests and response handlers, while the engine catches
// errors and makes sure that once there's nothing to do completeTest() is called.
// @param step is an object with command, params and callback fields
function runRequestSeries(step)
{
  processStep(step);

  function processStep(s)
  {
    try {
      processStepOrFail(s);
    } catch (e) {
      InspectorTest.log(e.stack);
      InspectorTest.completeTest();
    }
  }

  function processStepOrFail(s)
  {
    if (!s) {
      InspectorTest.completeTest();
      return;
    }
    if (!s.command) {
      // A simple loopback step.
      var next = s.callback();
      processStep(next);
      return;
    }

    var innerCallback = function(response)
    {
      if ("error" in response) {
        InspectorTest.log(response.error.message);
        InspectorTest.completeTest();
        return;
      }
      var next;
      try {
        next = s.callback(response.result);
      } catch (e) {
        InspectorTest.log(e.stack);
        InspectorTest.completeTest();
        return;
      }
      processStep(next);
    }
    var command = s.command.split(".");
    Protocol[command[0]][command[1]](s.params).then(innerCallback);
  }
}

var firstStep = { callback: callbackStart5 };

runRequestSeries(firstStep);

// 'Object5' section -- check properties of '5' wrapped as object  (has an internal property).

function callbackStart5()
{
  // Create an wrapper object with additional property.
  var expression = "(function(){var r = Object(5); r.foo = 'cat';return r;})()";

  return { command: "Runtime.evaluate", params: {expression: expression}, callback: callbackEval5 };
}
function callbackEval5(result)
{
  var id = result.result.objectId;
  if (id === undefined)
    throw new Error("objectId is expected");
  return {
    command: "Runtime.getProperties", params: {objectId: id, ownProperties: true}, callback: callbackProperties5
  };
}
function callbackProperties5(result)
{
  logGetPropertiesResult("Object(5)", result);
  return { callback: callbackStartNotOwn };
}


// 'Not own' section -- check all properties of the object, including ones from it prototype chain.

function callbackStartNotOwn()
{
  // Create an wrapper object with additional property.
  var expression = "({ a: 2, set b(_) {}, get b() {return 5;}, __proto__: { a: 3, c: 4, get d() {return 6;} }})";

  return { command: "Runtime.evaluate", params: {expression: expression}, callback: callbackEvalNotOwn };
}
function callbackEvalNotOwn(result)
{
  var id = result.result.objectId;
  if (id === undefined)
    throw new Error("objectId is expected");
  return {
    command: "Runtime.getProperties", params: {objectId: id, ownProperties: false}, callback: callbackPropertiesNotOwn
  };
}
function callbackPropertiesNotOwn(result)
{
  logGetPropertiesResult("Not own properties", result);
  return { callback: callbackStartAccessorsOnly };
}


// 'Accessors only' section -- check only accessor properties of the object.

function callbackStartAccessorsOnly()
{
  // Create an wrapper object with additional property.
  var expression = "({ a: 2, set b(_) {}, get b() {return 5;}, c: 'c', set d(_){} })";

  return { command: "Runtime.evaluate", params: {expression: expression}, callback: callbackEvalAccessorsOnly };
}
function callbackEvalAccessorsOnly(result)
{
  var id = result.result.objectId;
  if (id === undefined)
    throw new Error("objectId is expected");
  return {
    command: "Runtime.getProperties", params: {objectId: id, ownProperties: true, accessorPropertiesOnly: true}, callback: callbackPropertiesAccessorsOnly
  };
}
function callbackPropertiesAccessorsOnly(result)
{
  logGetPropertiesResult("Accessor only properties", result);
  return { callback: callbackStartArray };
}


// 'Array' section -- check properties of an array.

function callbackStartArray()
{
  var expression = "['red', 'green', 'blue']";
  return { command: "Runtime.evaluate", params: {expression: expression}, callback: callbackEvalArray };
}
function callbackEvalArray(result)
{
  var id = result.result.objectId;
  if (id === undefined)
    throw new Error("objectId is expected");
  return {
    command: "Runtime.getProperties", params: {objectId: id, ownProperties: true}, callback: callbackPropertiesArray
  };
}
function callbackPropertiesArray(result)
{
  logGetPropertiesResult("array", result);
  return { callback: callbackStartBound };
}


// 'Bound' section -- check properties of a bound function (has a bunch of internal properties).

function callbackStartBound()
{
  var expression = "Number.bind({}, 5)";
  return { command: "Runtime.evaluate", params: {expression: expression}, callback: callbackEvalBound };
}
function callbackEvalBound(result)
{
  var id = result.result.objectId;
  if (id === undefined)
    throw new Error("objectId is expected");
  return {
    command: "Runtime.getProperties", params: {objectId: id, ownProperties: true}, callback: callbackPropertiesBound
  };
}
function callbackPropertiesBound(result)
{
  logGetPropertiesResult("Bound function", result);
  return; // End of test
}

// A helper function that dumps object properties and internal properties in sorted order.
function logGetPropertiesResult(title, protocolResult)
{
  function hasGetterSetter(property, fieldName)
  {
    var v = property[fieldName];
    if (!v)
      return false;
    return v.type !== "undefined"
  }

  InspectorTest.log("Properties of " + title);
  var propertyArray = protocolResult.result;
  propertyArray.sort(NamedThingComparator);
  for (var i = 0; i < propertyArray.length; i++) {
    var p = propertyArray[i];
    var v = p.value;
    var own = p.isOwn ? "own" : "inherited";
    if (v)
      InspectorTest.log("  " + p.name + " " + own + " " + v.type + " " + v.value);
    else
      InspectorTest.log("  " + p.name + " " + own + " no value" +
        (hasGetterSetter(p, "get") ? ", getter" : "") + (hasGetterSetter(p, "set") ? ", setter" : ""));
  }
  var internalPropertyArray = protocolResult.internalProperties;
  if (internalPropertyArray) {
    InspectorTest.log("Internal properties");
    internalPropertyArray.sort(NamedThingComparator);
    for (var i = 0; i < internalPropertyArray.length; i++) {
      var p = internalPropertyArray[i];
      var v = p.value;
      InspectorTest.log("  " + p.name + " " + v.type + " " + v.value);
    }
  }

  function NamedThingComparator(o1, o2)
  {
    return o1.name === o2.name ? 0 : (o1.name < o2.name ? -1 : 1);
  }
}
