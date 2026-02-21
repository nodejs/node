// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that CommandLineAPI is presented only while evaluation.");

contextGroup.addScript(
`
var methods = ["dir","dirxml","profile","profileEnd","clear","table","keys","values","debug","undebug","monitor","unmonitor","inspect","copy"];
var window = this;
function presentedAPIMethods()
{
    var methodCount = 0;
    for (var method of methods) {
        try {
            if (eval("window." + method + "&&" + method + ".toString ? " + method + ".toString().indexOf(\\"[native code]\\") !== -1 : false"))
                ++methodCount;
        } catch (e) {
        }
    }
    methodCount += eval("\\"$_\\" in window ? $_ === 239 : false") ? 1 : 0;
    return methodCount;
}

function setPropertyForMethod()
{
    window.dir = 42;
}

function defineValuePropertyForMethod()
{
    Object.defineProperty(window, "dir", { value: 42 });
}

function defineAccessorPropertyForMethod()
{
    Object.defineProperty(window, "dir", { set: function() {}, get: function(){ return 42 } });
}

function definePropertiesForMethod()
{
    Object.defineProperties(window, { "dir": { set: function() {}, get: function(){ return 42 } }});
}

var builtinGetOwnPropertyDescriptorOnObject;
var builtinGetOwnPropertyDescriptorOnObjectPrototype;
var builtinGetOwnPropertyDescriptorOnWindow;

function redefineGetOwnPropertyDescriptors()
{
    builtinGetOwnPropertyDescriptorOnObject = Object.getOwnPropertyDescriptor;
    Object.getOwnPropertyDescriptor = function() {}
    builtinGetOwnPropertyDescriptorOnObjectPrototype = Object.prototype.getOwnPropertyDescriptor;
    Object.prototype.getOwnPropertyDescriptor = function() {}
    builtinGetOwnPropertyDescriptorOnWindow = window.getOwnPropertyDescriptor;
    window.getOwnPropertyDescriptor = function() {}
}

function restoreGetOwnPropertyDescriptors()
{
    Object.getOwnPropertyDescriptor = builtinGetOwnPropertyDescriptorOnObject;
    Object.prototype.getOwnPropertyDescriptor = builtinGetOwnPropertyDescriptorOnObjectPrototype;
    window.getOwnPropertyDescriptor = builtinGetOwnPropertyDescriptorOnWindow;
}`);

runExpressionAndDumpPresentedMethods("")
  .then(dumpLeftMethods)
  .then(() => runExpressionAndDumpPresentedMethods("setPropertyForMethod()"))
  .then(dumpLeftMethods)
  .then(dumpDir)
  .then(() => runExpressionAndDumpPresentedMethods("defineValuePropertyForMethod()"))
  .then(dumpLeftMethods)
  .then(dumpDir)
  .then(() => runExpressionAndDumpPresentedMethods("definePropertiesForMethod()"))
  .then(dumpLeftMethods)
  .then(dumpDir)
  .then(() => runExpressionAndDumpPresentedMethods("defineAccessorPropertyForMethod()"))
  .then(dumpLeftMethods)
  .then(dumpDir)
  .then(() => runExpressionAndDumpPresentedMethods("redefineGetOwnPropertyDescriptors()"))
  .then(dumpLeftMethods)
  .then(dumpDir)
  .then(() => evaluate("restoreGetOwnPropertyDescriptors()", false))
  .then(InspectorTest.completeTest);

function evaluate(expression, includeCommandLineAPI)
{
  return Protocol.Runtime.evaluate({ expression: expression, objectGroup: "console", includeCommandLineAPI: includeCommandLineAPI });
}

function setLastEvaluationResultTo239()
{
  return evaluate("239", false);
}

function runExpressionAndDumpPresentedMethods(expression)
{
  InspectorTest.log(expression);
  return setLastEvaluationResultTo239()
    .then(() => evaluate(expression + "; var a = presentedAPIMethods(); a", true))
    .then((result) => InspectorTest.logMessage(result));
}

function dumpLeftMethods()
{
  // Should always be zero.
  return setLastEvaluationResultTo239()
    .then(() => evaluate("presentedAPIMethods()", false))
    .then((result) => InspectorTest.logMessage(result));
}

function dumpDir()
{
  // Should always be presented.
  return evaluate("dir", false)
    .then((result) => InspectorTest.logMessage(result));
}
