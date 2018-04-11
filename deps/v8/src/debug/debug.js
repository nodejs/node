// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function (global, utils) {
"use strict";

// ----------------------------------------------------------------------------
// Imports

var FrameMirror = global.FrameMirror;
var GlobalArray = global.Array;
var GlobalRegExp = global.RegExp;
var IsNaN = global.isNaN;
var MakeMirror = global.MakeMirror;
var MathMin = global.Math.min;
var Mirror = global.Mirror;
var ValueMirror = global.ValueMirror;

//----------------------------------------------------------------------------

var Debug = {};

// Debug events which can occur in the V8 JavaScript engine. These originate
// from the API include file debug.h.
Debug.DebugEvent = { Break: 1,
                     Exception: 2,
                     AfterCompile: 3,
                     CompileError: 4,
                     AsyncTaskEvent: 5 };

// Types of exceptions that can be broken upon.
Debug.ExceptionBreak = { Caught : 0,
                         Uncaught: 1 };

// The different types of steps.
Debug.StepAction = { StepOut: 0,
                     StepNext: 1,
                     StepIn: 2 };

// The different types of scripts matching enum ScriptType in objects.h.
Debug.ScriptType = { Native: 0,
                     Extension: 1,
                     Normal: 2,
                     Wasm: 3};

// The different types of script compilations matching enum
// Script::CompilationType in objects.h.
Debug.ScriptCompilationType = { Host: 0,
                                Eval: 1,
                                JSON: 2 };

function ScriptTypeFlag(type) {
  return (1 << type);
}

// Globals.
var debugger_flags = {
  breakOnCaughtException: {
    getValue: function() { return Debug.isBreakOnException(); },
    setValue: function(value) {
      if (value) {
        Debug.setBreakOnException();
      } else {
        Debug.clearBreakOnException();
      }
    }
  },
  breakOnUncaughtException: {
    getValue: function() { return Debug.isBreakOnUncaughtException(); },
    setValue: function(value) {
      if (value) {
        Debug.setBreakOnUncaughtException();
      } else {
        Debug.clearBreakOnUncaughtException();
      }
    }
  },
};


// Returns a Script object. If the parameter is a function the return value
// is the script in which the function is defined. If the parameter is a string
// the return value is the script for which the script name has that string
// value.  If it is a regexp and there is a unique script whose name matches
// we return that, otherwise undefined.
Debug.findScript = function(func_or_script_name) {
  if (IS_FUNCTION(func_or_script_name)) {
    return %FunctionGetScript(func_or_script_name);
  } else if (%IsRegExp(func_or_script_name)) {
    var scripts = this.scripts();
    var last_result = null;
    var result_count = 0;
    for (var i in scripts) {
      var script = scripts[i];
      if (func_or_script_name.test(script.name)) {
        last_result = script;
        result_count++;
      }
    }
    // Return the unique script matching the regexp.  If there are more
    // than one we don't return a value since there is no good way to
    // decide which one to return.  Returning a "random" one, say the
    // first, would introduce nondeterminism (or something close to it)
    // because the order is the heap iteration order.
    if (result_count == 1) {
      return last_result;
    } else {
      return UNDEFINED;
    }
  } else {
    return %GetScript(func_or_script_name);
  }
};

// Returns the script source. If the parameter is a function the return value
// is the script source for the script in which the function is defined. If the
// parameter is a string the return value is the script for which the script
// name has that string value.
Debug.scriptSource = function(func_or_script_name) {
  return this.findScript(func_or_script_name).source;
};


Debug.source = function(f) {
  if (!IS_FUNCTION(f)) throw %make_type_error(kDebuggerType);
  return %FunctionGetSourceCode(f);
};


Debug.sourcePosition = function(f) {
  if (!IS_FUNCTION(f)) throw %make_type_error(kDebuggerType);
  return %FunctionGetScriptSourcePosition(f);
};


Debug.findFunctionSourceLocation = function(func, opt_line, opt_column) {
  var script = %FunctionGetScript(func);
  var script_offset = %FunctionGetScriptSourcePosition(func);
  return %ScriptLocationFromLine(script, opt_line, opt_column, script_offset);
};


// Returns the character position in a script based on a line number and an
// optional position within that line.
Debug.findScriptSourcePosition = function(script, opt_line, opt_column) {
  var location = %ScriptLocationFromLine(script, opt_line, opt_column, 0);
  return location ? location.position : null;
};

Debug.clearStepping = function() {
  %ClearStepping();
};

Debug.setBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Caught, true);
};

Debug.clearBreakOnException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Caught, false);
};

Debug.isBreakOnException = function() {
  return !!%IsBreakOnException(Debug.ExceptionBreak.Caught);
};

Debug.setBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, true);
};

Debug.clearBreakOnUncaughtException = function() {
  return %ChangeBreakOnException(Debug.ExceptionBreak.Uncaught, false);
};

Debug.isBreakOnUncaughtException = function() {
  return !!%IsBreakOnException(Debug.ExceptionBreak.Uncaught);
};

// Get all the scripts currently loaded. Locating all the scripts is based on
// scanning the heap.
Debug.scripts = function() {
  // Collect all scripts in the heap.
  return %DebugGetLoadedScripts();
};


// Get a specific script currently loaded. This is based on scanning the heap.
// TODO(clemensh): Create a runtime function for this.
function scriptById(scriptId) {
  var scripts = Debug.scripts();
  for (var script of scripts) {
    if (script.id == scriptId) return script;
  }
  return UNDEFINED;
};


Debug.debuggerFlags = function() {
  return debugger_flags;
};

Debug.MakeMirror = MakeMirror;

function MakeExecutionState(break_id) {
  return new ExecutionState(break_id);
}

function ExecutionState(break_id) {
  this.break_id = break_id;
  this.selected_frame = 0;
}

ExecutionState.prototype.prepareStep = function(action) {
  if (action === Debug.StepAction.StepIn ||
      action === Debug.StepAction.StepOut ||
      action === Debug.StepAction.StepNext) {
    return %PrepareStep(this.break_id, action);
  }
  throw %make_type_error(kDebuggerType);
};

ExecutionState.prototype.evaluateGlobal = function(source) {
  return MakeMirror(%DebugEvaluateGlobal(this.break_id, source));
};

ExecutionState.prototype.frameCount = function() {
  return %GetFrameCount(this.break_id);
};

ExecutionState.prototype.frame = function(opt_index) {
  // If no index supplied return the selected frame.
  if (opt_index == null) opt_index = this.selected_frame;
  if (opt_index < 0 || opt_index >= this.frameCount()) {
    throw %make_type_error(kDebuggerFrame);
  }
  return new FrameMirror(this.break_id, opt_index);
};

ExecutionState.prototype.setSelectedFrame = function(index) {
  var i = TO_NUMBER(index);
  if (i < 0 || i >= this.frameCount()) {
    throw %make_type_error(kDebuggerFrame);
  }
  this.selected_frame = i;
};

ExecutionState.prototype.selectedFrame = function() {
  return this.selected_frame;
};


function MakeExceptionEvent(break_id, exception, uncaught, promise) {
  return new ExceptionEvent(break_id, exception, uncaught, promise);
}


function ExceptionEvent(break_id, exception, uncaught, promise) {
  this.exec_state_ = new ExecutionState(break_id);
  this.exception_ = exception;
  this.uncaught_ = uncaught;
  this.promise_ = promise;
}


ExceptionEvent.prototype.eventType = function() {
  return Debug.DebugEvent.Exception;
};


ExceptionEvent.prototype.exception = function() {
  return this.exception_;
};


ExceptionEvent.prototype.uncaught = function() {
  return this.uncaught_;
};


ExceptionEvent.prototype.promise = function() {
  return this.promise_;
};


ExceptionEvent.prototype.func = function() {
  return this.exec_state_.frame(0).func();
};


ExceptionEvent.prototype.sourceLine = function() {
  return this.exec_state_.frame(0).sourceLine();
};


ExceptionEvent.prototype.sourceColumn = function() {
  return this.exec_state_.frame(0).sourceColumn();
};


ExceptionEvent.prototype.sourceLineText = function() {
  return this.exec_state_.frame(0).sourceLineText();
};


function MakeCompileEvent(script, type) {
  return new CompileEvent(script, type);
}


function CompileEvent(script, type) {
  this.script_ = MakeMirror(script);
  this.type_ = type;
}


CompileEvent.prototype.eventType = function() {
  return this.type_;
};


CompileEvent.prototype.script = function() {
  return this.script_;
};


function MakeScriptObject_(script, include_source) {
  var o = { id: script.id(),
            name: script.name(),
            lineOffset: script.lineOffset(),
            columnOffset: script.columnOffset(),
            lineCount: script.lineCount(),
          };
  if (!IS_UNDEFINED(script.data())) {
    o.data = script.data();
  }
  if (include_source) {
    o.source = script.source();
  }
  return o;
}


function MakeAsyncTaskEvent(type, id) {
  return new AsyncTaskEvent(type, id);
}


function AsyncTaskEvent(type, id) {
  this.type_ = type;
  this.id_ = id;
}


AsyncTaskEvent.prototype.type = function() {
  return this.type_;
}


AsyncTaskEvent.prototype.id = function() {
  return this.id_;
}

// -------------------------------------------------------------------
// Exports

utils.InstallConstants(global, [
  "Debug", Debug,
  "CompileEvent", CompileEvent,
]);

// Functions needed by the debugger runtime.
utils.InstallConstants(utils, [
  "MakeExecutionState", MakeExecutionState,
  "MakeExceptionEvent", MakeExceptionEvent,
  "MakeCompileEvent", MakeCompileEvent,
  "MakeAsyncTaskEvent", MakeAsyncTaskEvent,
]);

})
