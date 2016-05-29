/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
"use strict";

(function () {

var DebuggerScript = {};

/** @enum */
DebuggerScript.PauseOnExceptionsState = {
    DontPauseOnExceptions: 0,
    PauseOnAllExceptions: 1,
    PauseOnUncaughtExceptions: 2
};

DebuggerScript._pauseOnExceptionsState = DebuggerScript.PauseOnExceptionsState.DontPauseOnExceptions;
Debug.clearBreakOnException();
Debug.clearBreakOnUncaughtException();

/**
 * @param {!CompileEvent} eventData
 */
DebuggerScript.getAfterCompileScript = function(eventData)
{
    return DebuggerScript._formatScript(eventData.script().value());
}

/** @type {!Map<!ScopeType, string>} */
DebuggerScript._scopeTypeNames = new Map();
DebuggerScript._scopeTypeNames.set(ScopeType.Global, "global");
DebuggerScript._scopeTypeNames.set(ScopeType.Local, "local");
DebuggerScript._scopeTypeNames.set(ScopeType.With, "with");
DebuggerScript._scopeTypeNames.set(ScopeType.Closure, "closure");
DebuggerScript._scopeTypeNames.set(ScopeType.Catch, "catch");
DebuggerScript._scopeTypeNames.set(ScopeType.Block, "block");
DebuggerScript._scopeTypeNames.set(ScopeType.Script, "script");

/**
 * @param {function()} fun
 * @return {?Array<!Scope>}
 */
DebuggerScript.getFunctionScopes = function(fun)
{
    var mirror = MakeMirror(fun);
    if (!mirror.isFunction())
        return null;
    var functionMirror = /** @type {!FunctionMirror} */(mirror);
    var count = functionMirror.scopeCount();
    if (count == 0)
        return null;
    var result = [];
    for (var i = 0; i < count; i++) {
        var scopeDetails = functionMirror.scope(i).details();
        var scopeObject = DebuggerScript._buildScopeObject(scopeDetails.type(), scopeDetails.object());
        if (!scopeObject)
            continue;
        result.push({
            type: /** @type {string} */(DebuggerScript._scopeTypeNames.get(scopeDetails.type())),
            object: scopeObject,
            name: scopeDetails.name() || ""
        });
    }
    return result;
}

/**
 * @param {Object} object
 * @return {?GeneratorObjectDetails}
 */
DebuggerScript.getGeneratorObjectDetails = function(object)
{
    var mirror = MakeMirror(object, true /* transient */);
    if (!mirror.isGenerator())
        return null;
    var generatorMirror = /** @type {!GeneratorMirror} */(mirror);
    var funcMirror = generatorMirror.func();
    if (!funcMirror.resolved())
        return null;
    var result = {
        "function": funcMirror.value(),
        "functionName": funcMirror.debugName(),
        "status": generatorMirror.status()
    };
    var script = funcMirror.script();
    var location = generatorMirror.sourceLocation() || funcMirror.sourceLocation();
    if (script && location) {
        result["location"] = {
            "scriptId": String(script.id()),
            "lineNumber": location.line,
            "columnNumber": location.column
        };
    }
    return result;
}

/**
 * @param {Object} object
 * @return {!Array<!{value: *}>|undefined}
 */
DebuggerScript.getCollectionEntries = function(object)
{
    var mirror = MakeMirror(object, true /* transient */);
    if (mirror.isMap())
        return /** @type {!MapMirror} */(mirror).entries();
    if (mirror.isSet() || mirror.isIterator()) {
        var result = [];
        var values = mirror.isSet() ? /** @type {!SetMirror} */(mirror).values() : /** @type {!IteratorMirror} */(mirror).preview();
        for (var i = 0; i < values.length; ++i)
            result.push({ value: values[i] });
        return result;
    }
}

/**
 * @param {string|undefined} contextData
 * @return {number}
 */
DebuggerScript._executionContextId = function(contextData)
{
    if (!contextData)
        return 0;
    var firstComma = contextData.indexOf(",");
    if (firstComma === -1)
        return 0;
    var secondComma = contextData.indexOf(",", firstComma + 1);
    if (secondComma === -1)
        return 0;

    return parseInt(contextData.substring(firstComma + 1, secondComma), 10) || 0;
}

/**
 * @param {string} contextGroupId
 * @return {!Array<!FormattedScript>}
 */
DebuggerScript.getScripts = function(contextGroupId)
{
    var result = [];
    var scripts = Debug.scripts();
    var contextDataPrefix = null;
    if (contextGroupId)
        contextDataPrefix = contextGroupId + ",";
    for (var i = 0; i < scripts.length; ++i) {
        var script = scripts[i];
        if (contextDataPrefix) {
            if (!script.context_data)
                continue;
            // Context data is a string in the following format:
            // <contextGroupId>,<contextId>,("default"|"nondefault")
            if (script.context_data.indexOf(contextDataPrefix) !== 0)
                continue;
        }
        result.push(DebuggerScript._formatScript(script));
    }
    return result;
}

/**
 * @param {!Script} script
 * @return {!FormattedScript}
 */
DebuggerScript._formatScript = function(script)
{
    var lineEnds = script.line_ends;
    var lineCount = lineEnds.length;
    var endLine = script.line_offset + lineCount - 1;
    var endColumn;
    // V8 will not count last line if script source ends with \n.
    if (script.source[script.source.length - 1] === '\n') {
        endLine += 1;
        endColumn = 0;
    } else {
        if (lineCount === 1)
            endColumn = script.source.length + script.column_offset;
        else
            endColumn = script.source.length - (lineEnds[lineCount - 2] + 1);
    }
    return {
        id: script.id,
        name: script.nameOrSourceURL(),
        sourceURL: script.source_url,
        sourceMappingURL: script.source_mapping_url,
        source: script.source,
        startLine: script.line_offset,
        startColumn: script.column_offset,
        endLine: endLine,
        endColumn: endColumn,
        executionContextId: DebuggerScript._executionContextId(script.context_data),
        isContentScript: !!script.context_data && script.context_data.endsWith(",nondefault"),
        isInternalScript: script.is_debugger_script
    };
}

/**
 * @param {!ExecutionState} execState
 * @param {!BreakpointInfo} info
 * @return {string|undefined}
 */
DebuggerScript.setBreakpoint = function(execState, info)
{
    var positionAlignment = info.interstatementLocation ? Debug.BreakPositionAlignment.BreakPosition : Debug.BreakPositionAlignment.Statement;
    var breakId = Debug.setScriptBreakPointById(info.sourceID, info.lineNumber, info.columnNumber, info.condition, undefined, positionAlignment);

    var locations = Debug.findBreakPointActualLocations(breakId);
    if (!locations.length)
        return undefined;
    info.lineNumber = locations[0].line;
    info.columnNumber = locations[0].column;
    return breakId.toString();
}

/**
 * @param {!ExecutionState} execState
 * @param {!{breakpointId: number}} info
 */
DebuggerScript.removeBreakpoint = function(execState, info)
{
    Debug.findBreakPoint(info.breakpointId, true);
}

/**
 * @return {number}
 */
DebuggerScript.pauseOnExceptionsState = function()
{
    return DebuggerScript._pauseOnExceptionsState;
}

/**
 * @param {number} newState
 */
DebuggerScript.setPauseOnExceptionsState = function(newState)
{
    DebuggerScript._pauseOnExceptionsState = newState;

    if (DebuggerScript.PauseOnExceptionsState.PauseOnAllExceptions === newState)
        Debug.setBreakOnException();
    else
        Debug.clearBreakOnException();

    if (DebuggerScript.PauseOnExceptionsState.PauseOnUncaughtExceptions === newState)
        Debug.setBreakOnUncaughtException();
    else
        Debug.clearBreakOnUncaughtException();
}

/**
 * @param {!ExecutionState} execState
 * @param {number} limit
 * @return {!Array<!JavaScriptCallFrame>}
 */
DebuggerScript.currentCallFrames = function(execState, limit)
{
    var frames = [];
    for (var i = 0; i < execState.frameCount() && (!limit || i < limit); ++i)
        frames.push(DebuggerScript._frameMirrorToJSCallFrame(execState.frame(i)));
    return frames;
}

/**
 * @param {!ExecutionState} execState
 */
DebuggerScript.stepIntoStatement = function(execState)
{
    execState.prepareStep(Debug.StepAction.StepIn);
}

/**
 * @param {!ExecutionState} execState
 */
DebuggerScript.stepFrameStatement = function(execState)
{
    execState.prepareStep(Debug.StepAction.StepFrame);
}

/**
 * @param {!ExecutionState} execState
 */
DebuggerScript.stepOverStatement = function(execState)
{
    execState.prepareStep(Debug.StepAction.StepNext);
}

/**
 * @param {!ExecutionState} execState
 */
DebuggerScript.stepOutOfFunction = function(execState)
{
    execState.prepareStep(Debug.StepAction.StepOut);
}

DebuggerScript.clearStepping = function()
{
    Debug.clearStepping();
}

// Returns array in form:
//      [ 0, <v8_result_report> ] in case of success
//   or [ 1, <general_error_message>, <compiler_message>, <line_number>, <column_number> ] in case of compile error, numbers are 1-based.
// or throws exception with message.
/**
 * @param {number} scriptId
 * @param {string} newSource
 * @param {boolean} preview
 * @return {!Array<*>}
 */
DebuggerScript.liveEditScriptSource = function(scriptId, newSource, preview)
{
    var scripts = Debug.scripts();
    var scriptToEdit = null;
    for (var i = 0; i < scripts.length; i++) {
        if (scripts[i].id == scriptId) {
            scriptToEdit = scripts[i];
            break;
        }
    }
    if (!scriptToEdit)
        throw("Script not found");

    var changeLog = [];
    try {
        var result = Debug.LiveEdit.SetScriptSource(scriptToEdit, newSource, preview, changeLog);
        return [0, result.stack_modified];
    } catch (e) {
        if (e instanceof Debug.LiveEdit.Failure && "details" in e) {
            var details = /** @type {!LiveEditErrorDetails} */(e.details);
            if (details.type === "liveedit_compile_error") {
                var startPosition = details.position.start;
                return [1, String(e), String(details.syntaxErrorMessage), Number(startPosition.line), Number(startPosition.column)];
            }
        }
        throw e;
    }
}

/**
 * @param {!ExecutionState} execState
 */
DebuggerScript.clearBreakpoints = function(execState)
{
    Debug.clearAllBreakPoints();
}

/**
 * @param {!ExecutionState} execState
 * @param {!{enabled: boolean}} info
 */
DebuggerScript.setBreakpointsActivated = function(execState, info)
{
    Debug.debuggerFlags().breakPointsActive.setValue(info.enabled);
}

/**
 * @param {!BreakEvent} eventData
 */
DebuggerScript.getBreakpointNumbers = function(eventData)
{
    var breakpoints = eventData.breakPointsHit();
    var numbers = [];
    if (!breakpoints)
        return numbers;

    for (var i = 0; i < breakpoints.length; i++) {
        var breakpoint = breakpoints[i];
        var scriptBreakPoint = breakpoint.script_break_point();
        numbers.push(scriptBreakPoint ? scriptBreakPoint.number() : breakpoint.number());
    }
    return numbers;
}

// NOTE: This function is performance critical, as it can be run on every
// statement that generates an async event (like addEventListener) to support
// asynchronous call stacks. Thus, when possible, initialize the data lazily.
/**
 * @param {!FrameMirror} frameMirror
 * @return {!JavaScriptCallFrame}
 */
DebuggerScript._frameMirrorToJSCallFrame = function(frameMirror)
{
    // Stuff that can not be initialized lazily (i.e. valid while paused with a valid break_id).
    // The frameMirror and scopeMirror can be accessed only while paused on the debugger.
    var frameDetails = frameMirror.details();

    var funcObject = frameDetails.func();
    var sourcePosition = frameDetails.sourcePosition();
    var thisObject = frameDetails.receiver();

    var isAtReturn = !!frameDetails.isAtReturn();
    var returnValue = isAtReturn ? frameDetails.returnValue() : undefined;

    var scopeMirrors = frameMirror.allScopes(false);
    /** @type {!Array<ScopeType>} */
    var scopeTypes = new Array(scopeMirrors.length);
    /** @type {?Array<!Object>} */
    var scopeObjects = new Array(scopeMirrors.length);
    /** @type {!Array<string|undefined>} */
    var scopeNames = new Array(scopeMirrors.length);
    /** @type {?Array<number>} */
    var scopeStartPositions = new Array(scopeMirrors.length);
    /** @type {?Array<number>} */
    var scopeEndPositions = new Array(scopeMirrors.length);
    /** @type {?Array<function()|null>} */
    var scopeFunctions = new Array(scopeMirrors.length);
    for (var i = 0; i < scopeMirrors.length; ++i) {
        var scopeDetails = scopeMirrors[i].details();
        scopeTypes[i] = scopeDetails.type();
        scopeObjects[i] = scopeDetails.object();
        scopeNames[i] = scopeDetails.name();
        scopeStartPositions[i] = scopeDetails.startPosition ? scopeDetails.startPosition() : 0;
        scopeEndPositions[i] = scopeDetails.endPosition ? scopeDetails.endPosition() : 0;
        scopeFunctions[i] = scopeDetails.func ? scopeDetails.func() : null;
    }

    // Calculated lazily.
    var scopeChain;
    var funcMirror;
    var location;
    /** @type {!Array<?RawLocation>} */
    var scopeStartLocations;
    /** @type {!Array<?RawLocation>} */
    var scopeEndLocations;
    var details;

    /**
     * @param {!ScriptMirror|undefined} script
     * @param {number} pos
     * @return {?RawLocation}
     */
    function createLocation(script, pos)
    {
        if (!script)
            return null;

        var location = script.locationFromPosition(pos, true);
        return {
            "lineNumber": location.line,
            "columnNumber": location.column,
            "scriptId": String(script.id())
        }
    }

    /**
     * @return {!Array<!Object>}
     */
    function ensureScopeChain()
    {
        if (!scopeChain) {
            scopeChain = [];
            scopeStartLocations = [];
            scopeEndLocations = [];
            for (var i = 0, j = 0; i < scopeObjects.length; ++i) {
                var scopeObject = DebuggerScript._buildScopeObject(scopeTypes[i], scopeObjects[i]);
                if (scopeObject) {
                    scopeTypes[j] = scopeTypes[i];
                    scopeNames[j] = scopeNames[i];
                    scopeChain[j] = scopeObject;

                    var funcMirror = scopeFunctions ? MakeMirror(scopeFunctions[i]) : null;
                    if (!funcMirror || !funcMirror.isFunction())
                        funcMirror = new UnresolvedFunctionMirror(funcObject);

                    var script = /** @type {!FunctionMirror} */(funcMirror).script();
                    scopeStartLocations[j] = createLocation(script, scopeStartPositions[i]);
                    scopeEndLocations[j] = createLocation(script, scopeEndPositions[i]);
                    ++j;
                }
            }
            scopeTypes.length = scopeChain.length;
            scopeNames.length = scopeChain.length;
            scopeObjects = null; // Free for GC.
            scopeFunctions = null;
            scopeStartPositions = null;
            scopeEndPositions = null;
        }
        return scopeChain;
    }

    /**
     * @return {!JavaScriptCallFrameDetails}
     */
    function lazyDetails()
    {
        if (!details) {
            var scopeObjects = ensureScopeChain();
            var script = ensureFuncMirror().script();
            /** @type {!Array<Scope>} */
            var scopes = [];
            for (var i = 0; i < scopeObjects.length; ++i) {
                var scope = {
                    "type": /** @type {string} */(DebuggerScript._scopeTypeNames.get(scopeTypes[i])),
                    "object": scopeObjects[i],
                };
                if (scopeNames[i])
                    scope.name = scopeNames[i];
                if (scopeStartLocations[i])
                    scope.startLocation = /** @type {!RawLocation} */(scopeStartLocations[i]);
                if (scopeEndLocations[i])
                    scope.endLocation = /** @type {!RawLocation} */(scopeEndLocations[i]);
                scopes.push(scope);
            }
            details = {
                "functionName": ensureFuncMirror().debugName(),
                "location": {
                    "lineNumber": line(),
                    "columnNumber": column(),
                    "scriptId": String(script.id())
                },
                "this": thisObject,
                "scopeChain": scopes
            };
            var functionLocation = ensureFuncMirror().sourceLocation();
            if (functionLocation) {
                details.functionLocation = {
                    "lineNumber": functionLocation.line,
                    "columnNumber": functionLocation.column,
                    "scriptId": String(script.id())
                };
            }
            if (isAtReturn)
                details.returnValue = returnValue;
        }
        return details;
    }

    /**
     * @return {!FunctionMirror}
     */
    function ensureFuncMirror()
    {
        if (!funcMirror) {
            funcMirror = MakeMirror(funcObject);
            if (!funcMirror.isFunction())
                funcMirror = new UnresolvedFunctionMirror(funcObject);
        }
        return /** @type {!FunctionMirror} */(funcMirror);
    }

    /**
     * @return {!{line: number, column: number}}
     */
    function ensureLocation()
    {
        if (!location) {
            var script = ensureFuncMirror().script();
            if (script)
                location = script.locationFromPosition(sourcePosition, true);
            if (!location)
                location = { line: 0, column: 0 };
        }
        return location;
    }

    /**
     * @return {number}
     */
    function line()
    {
        return ensureLocation().line;
    }

    /**
     * @return {number}
     */
    function column()
    {
        return ensureLocation().column;
    }

    /**
     * @return {number}
     */
    function contextId()
    {
        var mirror = ensureFuncMirror();
        // Old V8 do not have context() function on these objects
        if (!mirror.context)
            return DebuggerScript._executionContextId(mirror.script().value().context_data);
        var context = mirror.context();
        if (context)
            return DebuggerScript._executionContextId(context.data());
        return 0;
    }

    /**
     * @return {number|undefined}
     */
    function sourceID()
    {
        var script = ensureFuncMirror().script();
        return script && script.id();
    }

    /**
     * @param {string} expression
     * @return {*}
     */
    function evaluate(expression)
    {
        return frameMirror.evaluate(expression, false).value();
    }

    /** @return {undefined} */
    function restart()
    {
        return frameMirror.restart();
    }

    /**
     * @param {number} scopeNumber
     * @param {string} variableName
     * @param {*} newValue
     */
    function setVariableValue(scopeNumber, variableName, newValue)
    {
        var scopeMirror = frameMirror.scope(scopeNumber);
        if (!scopeMirror)
            throw new Error("Incorrect scope index");
        scopeMirror.setVariableValue(variableName, newValue);
    }

    return {
        "sourceID": sourceID,
        "line": line,
        "column": column,
        "contextId": contextId,
        "thisObject": thisObject,
        "evaluate": evaluate,
        "restart": restart,
        "setVariableValue": setVariableValue,
        "isAtReturn": isAtReturn,
        "details": lazyDetails
    };
}

/**
 * @param {number} scopeType
 * @param {!Object} scopeObject
 * @return {!Object|undefined}
 */
DebuggerScript._buildScopeObject = function(scopeType, scopeObject)
{
    var result;
    switch (scopeType) {
    case ScopeType.Local:
    case ScopeType.Closure:
    case ScopeType.Catch:
    case ScopeType.Block:
    case ScopeType.Script:
        // For transient objects we create a "persistent" copy that contains
        // the same properties.
        // Reset scope object prototype to null so that the proto properties
        // don't appear in the local scope section.
        var properties = /** @type {!ObjectMirror} */(MakeMirror(scopeObject, true /* transient */)).properties();
        // Almost always Script scope will be empty, so just filter out that noise.
        // Also drop empty Block scopes, should we get any.
        if (!properties.length && (scopeType === ScopeType.Script || scopeType === ScopeType.Block))
            break;
        result = { __proto__: null };
        for (var j = 0; j < properties.length; j++) {
            var name = properties[j].name();
            if (name.length === 0 || name.charAt(0) === ".")
                continue; // Skip internal variables like ".arguments" and variables with empty name
            result[name] = properties[j].value_;
        }
        break;
    case ScopeType.Global:
    case ScopeType.With:
        result = scopeObject;
        break;
    }
    return result;
}

// We never resolve Mirror by its handle so to avoid memory leaks caused by Mirrors in the cache we disable it.
ToggleMirrorCache(false);

return DebuggerScript;
})();
