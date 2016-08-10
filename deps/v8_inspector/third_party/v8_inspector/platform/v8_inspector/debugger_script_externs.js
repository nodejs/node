// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{
        type: string,
        object: !Object,
        name: (string|undefined),
        startLocation: (!RawLocation|undefined),
        endLocation: (!RawLocation|undefined)
    }} */
var Scope;

/** @typedef {{
        scriptId: string,
        lineNumber: number,
        columnNumber: number
    }} */
var RawLocation;

/** @typedef {{
        id: number,
        name: string,
        sourceURL: (string|undefined),
        sourceMappingURL: (string|undefined),
        source: string,
        startLine: number,
        endLine: number,
        startColumn: number,
        endColumn: number,
        executionContextId: number,
        executionContextAuxData: string,
        isInternalScript: boolean
    }} */
var FormattedScript;

/** @typedef {{
        functionName: string,
        location: !RawLocation,
        this: !Object,
        scopeChain: !Array<!Scope>,
        functionLocation: (RawLocation|undefined),
        returnValue: (*|undefined)
    }} */
var JavaScriptCallFrameDetails;

/** @typedef {{
        sourceID: function():(number|undefined),
        line: function():number,
        column: function():number,
        thisObject: !Object,
        evaluate: function(string):*,
        restart: function():undefined,
        setVariableValue: function(number, string, *):undefined,
        isAtReturn: boolean,
        details: function():!JavaScriptCallFrameDetails
    }} */
var JavaScriptCallFrame;

/** @interface */
function DebugClass()
{
    /** @type {!LiveEditClass} */
    this.LiveEdit;
}

DebugClass.prototype.setBreakOnException = function() {}

DebugClass.prototype.clearBreakOnException = function() {}

DebugClass.prototype.setBreakOnUncaughtException = function() {}

DebugClass.prototype.clearBreakOnUncaughtException = function() {}

DebugClass.prototype.clearStepping = function() {}

DebugClass.prototype.clearAllBreakPoints = function() {}

/** @return {!Array<!Script>} */
DebugClass.prototype.scripts = function() {}

/**
 * @param {number} scriptId
 * @param {number=} line
 * @param {number=} column
 * @param {string=} condition
 * @param {string=} groupId
 * @param {Debug.BreakPositionAlignment=} positionAlignment
 */
DebugClass.prototype.setScriptBreakPointById = function(scriptId, line, column, condition, groupId, positionAlignment) {}

/**
 * @param {number} breakId
 * @return {!Array<!SourceLocation>}
 */
DebugClass.prototype.findBreakPointActualLocations = function(breakId) {}

/**
 * @param {number} breakId
 * @param {boolean} remove
 * @return {!BreakPoint|undefined}
 */
DebugClass.prototype.findBreakPoint = function(breakId, remove) {}

/** @return {!DebuggerFlags} */
DebugClass.prototype.debuggerFlags = function() {}

/** @type {!DebugClass} */
var Debug;


/** @enum */
Debug.BreakPositionAlignment = {
    Statement: 0,
    BreakPosition: 1
};

/** @enum */
Debug.StepAction = { StepOut: 0,
                     StepNext: 1,
                     StepIn: 2,
                     StepFrame: 3 };

/** @enum */
Debug.ScriptCompilationType = { Host: 0,
                                Eval: 1,
                                JSON: 2 };


/** @interface */
function DebuggerFlag() {}

/** @param {boolean} value */
DebuggerFlag.prototype.setValue = function(value) {}


/** @interface */
function DebuggerFlags()
{
    /** @type {!DebuggerFlag} */
    this.breakPointsActive;
}


/** @interface */
function LiveEditClass() {}

/**
 * @param {!Script} script
 * @param {string} newSource
 * @param {boolean} previewOnly
 * @return {!{stack_modified: (boolean|undefined)}}
 */
LiveEditClass.prototype.SetScriptSource = function(script, newSource, previewOnly, change_log) {}


/** @interface */
function LiveEditErrorDetails()
{
  /** @type {string} */
  this.syntaxErrorMessage;
  /** @type {!{start: !{line: number, column: number}}} */
  this.position;
}


/** @interface */
function BreakpointInfo()
{
    /** @type {number} */
    this.breakpointId;
    /** @type {number} */
    this.sourceID;
    /** @type {number|undefined} */
    this.lineNumber;
    /** @type {number|undefined} */
    this.columnNumber;
    /** @type {string|undefined} */
    this.condition;
    /** @type {boolean|undefined} */
    this.interstatementLocation;
}


/** @interface */
function BreakPoint() {}

/** @return {!BreakPoint|undefined} */
BreakPoint.prototype.script_break_point = function() {}

/** @return {number} */
BreakPoint.prototype.number = function() {}


/** @interface */
function CompileEvent() {}

/** @return {!ScriptMirror} */
CompileEvent.prototype.script = function() {}


/** @interface */
function BreakEvent() {}

/** @return {!Array<!BreakPoint>|undefined} */
BreakEvent.prototype.breakPointsHit = function() {}


/** @interface */
function ExecutionState() {}

/** @param {!Debug.StepAction} action */
ExecutionState.prototype.prepareStep = function(action) {}

/**
 * @param {string} source
 * @param {boolean} disableBreak
 * @param {*=} additionalContext
 */
ExecutionState.prototype.evaluateGlobal = function(source, disableBreak, additionalContext) {}

/** @return {number} */
ExecutionState.prototype.frameCount = function() {}

/**
 * @param {number} index
 * @return {!FrameMirror}
 */
ExecutionState.prototype.frame = function(index) {}

/** @param {number} index */
ExecutionState.prototype.setSelectedFrame = function(index) {}

/** @return {number} */
ExecutionState.prototype.selectedFrame = function() {}


/** @enum */
var ScopeType = { Global: 0,
                  Local: 1,
                  With: 2,
                  Closure: 3,
                  Catch: 4,
                  Block: 5,
                  Script: 6 };


/** @interface */
function SourceLocation()
{
    /** @type {number} */
    this.script;
    /** @type {number} */
    this.position;
    /** @type {number} */
    this.line;
    /** @type {number} */
    this.column;
    /** @type {number} */
    this.start;
    /** @type {number} */
    this.end;
}


/** @interface */
function Script()
{
    /** @type {number} */
    this.id;
    /** @type {string|undefined} */
    this.context_data;
    /** @type {string|undefined} */
    this.source_url;
    /** @type {string|undefined} */
    this.source_mapping_url;
    /** @type {boolean} */
    this.is_debugger_script;
    /** @type {string} */
    this.source;
    /** @type {!Array<number>} */
    this.line_ends;
    /** @type {number} */
    this.line_offset;
    /** @type {number} */
    this.column_offset;
}

/** @return {string} */
Script.prototype.nameOrSourceURL = function() {}

/** @return {!Debug.ScriptCompilationType} */
Script.prototype.compilationType = function() {}


/** @interface */
function ScopeDetails() {}

/** @return {!Object} */
ScopeDetails.prototype.object = function() {}

/** @return {string|undefined} */
ScopeDetails.prototype.name = function() {}


/** @interface */
function FrameDetails() {}

/** @return {!Object} */
FrameDetails.prototype.receiver = function() {}

/** @return {function()} */
FrameDetails.prototype.func = function() {}

/** @return {boolean} */
FrameDetails.prototype.isAtReturn = function() {}

/** @return {number} */
FrameDetails.prototype.sourcePosition = function() {}

/** @return {*} */
FrameDetails.prototype.returnValue = function() {}

/** @return {number} */
FrameDetails.prototype.scopeCount = function() {}


/** @param {boolean} value */
function ToggleMirrorCache(value) {}

/**
 * @param {*} value
 * @param {boolean=} transient
 * @return {!Mirror}
 */
function MakeMirror(value, transient) {}


/** @interface */
function Mirror() {}

/** @return {boolean} */
Mirror.prototype.isFunction = function() {}

/** @return {boolean} */
Mirror.prototype.isGenerator = function() {}

/** @return {boolean} */
Mirror.prototype.isMap = function() {}

/** @return {boolean} */
Mirror.prototype.isSet = function() {}

/** @return {boolean} */
Mirror.prototype.isIterator = function() {}


/**
 * @interface
 * @extends {Mirror}
 */
function ObjectMirror() {}

/** @return {!Array<!PropertyMirror>} */
ObjectMirror.prototype.properties = function() {}


/**
 * @interface
 * @extends {ObjectMirror}
 */
function FunctionMirror () {}

/** @return {number} */
FunctionMirror.prototype.scopeCount = function() {}

/**
 * @param {number} index
 * @return {!ScopeMirror|undefined}
 */
FunctionMirror.prototype.scope = function(index) {}

/** @return {boolean} */
FunctionMirror.prototype.resolved = function() {}

/** @return {function()} */
FunctionMirror.prototype.value = function() {}

/** @return {string} */
FunctionMirror.prototype.debugName = function() {}

/** @return {!ScriptMirror|undefined} */
FunctionMirror.prototype.script = function() {}

/** @return {!SourceLocation|undefined} */
FunctionMirror.prototype.sourceLocation = function() {}

/** @return {!ContextMirror|undefined} */
FunctionMirror.prototype.context = function() {}

/**
 * @constructor
 * @param {*} value
 */
function UnresolvedFunctionMirror(value) {}


/**
 * @interface
 * @extends {ObjectMirror}
 */
function MapMirror () {}

/**
 * @param {number=} limit
 * @return {!Array<!{key: *, value: *}>}
 */
MapMirror.prototype.entries = function(limit) {}


/**
 * @interface
 * @extends {ObjectMirror}
 */
function SetMirror () {}

/**
 * @param {number=} limit
 * @return {!Array<*>}
 */
SetMirror.prototype.values = function(limit) {}


/**
 * @interface
 * @extends {ObjectMirror}
 */
function IteratorMirror () {}

/**
 * @param {number=} limit
 * @return {!Array<*>}
 */
IteratorMirror.prototype.preview = function(limit) {}


/**
 * @interface
 * @extends {ObjectMirror}
 */
function GeneratorMirror () {}

/** @return {string} */
GeneratorMirror.prototype.status = function() {}

/** @return {!SourceLocation|undefined} */
GeneratorMirror.prototype.sourceLocation = function() {}

/** @return {!FunctionMirror} */
GeneratorMirror.prototype.func = function() {}


/**
 * @interface
 * @extends {Mirror}
 */
function PropertyMirror()
{
    /** @type {*} */
    this.value_;
}

/** @return {!Mirror} */
PropertyMirror.prototype.value = function() {}

/** @return {string} */
PropertyMirror.prototype.name = function() {}


/**
 * @interface
 * @extends {Mirror}
 */
function FrameMirror() {}

/**
 * @param {boolean=} ignoreNestedScopes
 * @return {!Array<!ScopeMirror>}
 */
FrameMirror.prototype.allScopes = function(ignoreNestedScopes) {}

/** @return {!FrameDetails} */
FrameMirror.prototype.details = function() {}

/**
 * @param {string} source
 * @param {boolean} disableBreak
 */
FrameMirror.prototype.evaluate = function(source, disableBreak) {}

FrameMirror.prototype.restart = function() {}

/** @param {number} index */
FrameMirror.prototype.scope = function(index) {}


/**
 * @interface
 * @extends {Mirror}
 */
function ScriptMirror() {}

/** @return {!Script} */
ScriptMirror.prototype.value = function() {}

/** @return {number} */
ScriptMirror.prototype.id = function() {}

/**
 * @param {number} position
 * @param {boolean=} includeResourceOffset
 */
ScriptMirror.prototype.locationFromPosition = function(position, includeResourceOffset) {}


/**
 * @interface
 * @extends {Mirror}
 */
function ScopeMirror() {}

/** @return {!ScopeDetails} */
ScopeMirror.prototype.details = function() {}

/**
 * @param {string} name
 * @param {*} newValue
 */
ScopeMirror.prototype.setVariableValue = function(name, newValue) {}

/**
 * @interface
 * @extends {Mirror}
 */
function ContextMirror() {}

/** @return {string|undefined} */
ContextMirror.prototype.data = function() {}
