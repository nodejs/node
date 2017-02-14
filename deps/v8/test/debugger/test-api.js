// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// If true, prints all messages sent and received by inspector.
const printProtocolMessages = false;

// The active wrapper instance.
let activeWrapper = undefined;

// Receiver function called by inspector, delegating to active wrapper.
function receive(message) {
  activeWrapper.receiveMessage(message);
}

class DebugWrapper {
  constructor() {
    // Message dictionary storing {id, message} pairs.
    this.receivedMessages = new Map();

    // Each message dispatched by the Debug wrapper is assigned a unique number
    // using nextMessageId.
    this.nextMessageId = 0;

    // The listener method called on certain events.
    this.listener = undefined;

    // TODO(jgruber): Determine which of these are still required and possible.
    // Debug events which can occur in the V8 JavaScript engine.
    this.DebugEvent = { Break: 1,
                        Exception: 2,
                        NewFunction: 3,
                        BeforeCompile: 4,
                        AfterCompile: 5,
                        CompileError: 6,
                        AsyncTaskEvent: 7
                      };

    // The different types of steps.
    this.StepAction = { StepOut: 0,
                        StepNext: 1,
                        StepIn: 2,
                        StepFrame: 3,
                      };

    // A copy of the scope types from runtime-debug.cc.
    // NOTE: these constants should be backward-compatible, so
    // add new ones to the end of this list.
    this.ScopeType = { Global:  0,
                       Local:   1,
                       With:    2,
                       Closure: 3,
                       Catch:   4,
                       Block:   5,
                       Script:  6,
                       Eval:    7,
                       Module:  8
                     };

    // Types of exceptions that can be broken upon.
    this.ExceptionBreak = { Caught : 0,
                            Uncaught: 1 };

    // Store the current script id so we can skip corresponding break events.
    this.thisScriptId = %FunctionGetScriptId(receive);

    // Register as the active wrapper.
    assertTrue(activeWrapper === undefined);
    activeWrapper = this;
  }

  enable() { this.sendMessageForMethodChecked("Debugger.enable"); }
  disable() { this.sendMessageForMethodChecked("Debugger.disable"); }

  setListener(listener) { this.listener = listener; }

  stepOver() { this.sendMessageForMethodChecked("Debugger.stepOver"); }
  stepInto() { this.sendMessageForMethodChecked("Debugger.stepInto"); }
  stepOut() { this.sendMessageForMethodChecked("Debugger.stepOut"); }

  setBreakOnException()  {
    this.sendMessageForMethodChecked(
        "Debugger.setPauseOnExceptions", { state : "all" });
  }

  clearBreakOnException()  {
    const newState = this.isBreakOnUncaughtException() ? "uncaught" : "none";
    this.sendMessageForMethodChecked(
        "Debugger.setPauseOnExceptions", { state : newState });
  }

  isBreakOnException() {
    return !!%IsBreakOnException(this.ExceptionBreak.Caught);
  };

  setBreakOnUncaughtException()  {
    const newState = this.isBreakOnException() ? "all" : "uncaught";
    this.sendMessageForMethodChecked(
        "Debugger.setPauseOnExceptions", { state : newState });
  }

  clearBreakOnUncaughtException()  {
    const newState = this.isBreakOnException() ? "all" : "none";
    this.sendMessageForMethodChecked(
        "Debugger.setPauseOnExceptions", { state : newState });
  }

  isBreakOnUncaughtException() {
    return !!%IsBreakOnException(this.ExceptionBreak.Uncaught);
  };

  clearStepping() { %ClearStepping(); };

  // Returns the resulting breakpoint id.
  setBreakPoint(func, opt_line, opt_column, opt_condition) {
    assertTrue(%IsFunction(func));
    assertFalse(%FunctionIsAPIFunction(func));

    // TODO(jgruber): We handle only script breakpoints for now.

    const scriptid = %FunctionGetScriptId(func);
    assertTrue(scriptid != -1);

    const offset = %FunctionGetScriptSourcePosition(func);
    const loc =
      %ScriptLocationFromLine2(scriptid, opt_line, opt_column, offset);

    const params = { location :
                       { scriptId : scriptid.toString(),
                         lineNumber : loc.line,
                         columnNumber : loc.column,
                       }};
    if (!!opt_condition) {
      params.condition = opt_condition;
    }

    const {msgid, msg} = this.createMessage(
        "Debugger.setBreakpoint", params);
    this.sendMessage(msg);

    const reply = this.takeReplyChecked(msgid);
    assertTrue(reply.result !== undefined);
    const breakid = reply.result.breakpointId;
    assertTrue(breakid !== undefined);

    return breakid;
  }

  clearBreakPoint(breakid) {
    const {msgid, msg} = this.createMessage(
        "Debugger.removeBreakpoint", { breakpointId : breakid });
    this.sendMessage(msg);
    this.takeReplyChecked(msgid);
  }

  // Returns the serialized result of the given expression. For example:
  // {"type":"number", "value":33, "description":"33"}.
  evaluate(frameid, expression) {
    const {msgid, msg} = this.createMessage(
        "Debugger.evaluateOnCallFrame",
        { callFrameId : frameid,
          expression : expression
        });
    this.sendMessage(msg);

    const reply = this.takeReplyChecked(msgid);
    return reply.result.result;
  }

  // --- Internal methods. -----------------------------------------------------

  getNextMessageId() {
    return this.nextMessageId++;
  }

  createMessage(method, params) {
    const id = this.getNextMessageId();
    const msg = JSON.stringify({
      id: id,
      method: method,
      params: params,
    });
    return { msgid : id, msg: msg };
  }

  receiveMessage(message) {
    if (printProtocolMessages) print(message);

    const parsedMessage = JSON.parse(message);
    if (parsedMessage.id !== undefined) {
      this.receivedMessages.set(parsedMessage.id, parsedMessage);
    }

    this.dispatchMessage(parsedMessage);
  }

  sendMessage(message) {
    if (printProtocolMessages) print(message);
    send(message);
  }

  sendMessageForMethodChecked(method, params) {
    const {msgid, msg} = this.createMessage(method, params);
    this.sendMessage(msg);
    this.takeReplyChecked(msgid);
  }

  takeReplyChecked(msgid) {
    const reply = this.receivedMessages.get(msgid);
    assertTrue(reply !== undefined);
    this.receivedMessages.delete(msgid);
    return reply;
  }

  execStatePrepareStep(action) {
    switch(action) {
      case this.StepAction.StepOut: this.stepOut(); break;
      case this.StepAction.StepNext: this.stepOver(); break;
      case this.StepAction.StepIn: this.stepInto(); break;
      default: %AbortJS("Unsupported StepAction"); break;
    }
  }

  execStateScopeType(type) {
    switch (type) {
      case "global": return this.ScopeType.Global;
      case "local": return this.ScopeType.Local;
      case "with": return this.ScopeType.With;
      case "closure": return this.ScopeType.Closure;
      case "catch": return this.ScopeType.Catch;
      case "block": return this.ScopeType.Block;
      case "script": return this.ScopeType.Script;
      default: %AbortJS("Unexpected scope type");
    }
  }

  // Returns an array of property descriptors of the scope object.
  // This is in contrast to the original API, which simply passed object
  // mirrors.
  execStateScopeObject(obj) {
    const serialized_scope = this.getProperties(obj.objectId);
    const scope = {}
    const scope_tuples = serialized_scope.forEach((elem) => {
      const key = elem.name;

      let value;
      if (elem.value) {
        // Some properties (e.g. with getters/setters) don't have a value.
        switch (elem.value.type) {
          case "undefined": value = undefined; break;
          default: value = elem.value.value; break;
        }
      }

      scope[key] = value;
    })

    return { value : () => scope };
  }

  execStateScope(scope) {
    return { scopeType : () => this.execStateScopeType(scope.type),
             scopeObject : () => this.execStateScopeObject(scope.object)
           };
  }

  getProperties(objectId) {
    const {msgid, msg} = this.createMessage(
        "Runtime.getProperties", { objectId : objectId });
    this.sendMessage(msg);
    const reply = this.takeReplyChecked(msgid);
    return reply.result.result;
  }

  getLocalScopeDetails(frame) {
    const scopes = frame.scopeChain;
    for (let i = 0; i < scopes.length; i++) {
      const scope = scopes[i]
      if (scope.type == "local") {
        return this.getProperties(scope.object.objectId);
      }
    }

    return undefined;
  }

  execStateFrameLocalCount(frame) {
    const scope_details = this.getLocalScopeDetails(frame);
    return scope_details ? scope_details.length : 0;
  }

  execStateFrameLocalName(frame, index) {
    const scope_details = this.getLocalScopeDetails(frame);
    if (index < 0 || index >= scope_details.length) return undefined;
    return scope_details[index].name;
  }

  execStateFrameLocalValue(frame, index) {
    const scope_details = this.getLocalScopeDetails(frame);
    if (index < 0 || index >= scope_details.length) return undefined;

    const local = scope_details[index];

    let localValue;
    switch (local.value.type) {
      case "undefined": localValue = undefined; break;
      default: localValue = local.value.value; break;
    }

    return { value : () => localValue };
  }

  execStateFrameEvaluate(frame, expr) {
    const frameid = frame.callFrameId;
    const {msgid, msg} = this.createMessage(
        "Debugger.evaluateOnCallFrame",
        { callFrameId : frameid,
          expression : expr
        });
    this.sendMessage(msg);
    const reply = this.takeReplyChecked(msgid);

    const result = reply.result.result;
    if (result.subtype == "error") {
      throw new Error(result.description);
    }

    return { value : () => result.value };
  }

  execStateFrame(frame) {
    const scriptid = parseInt(frame.location.scriptId);
    const line = frame.location.lineNumber;
    const column = frame.location.columnNumber;
    const loc = %ScriptLocationFromLine2(scriptid, line, column, 0);
    const func = { name : () => frame.functionName };
    return { sourceLineText : () => loc.sourceText,
             evaluate : (expr) => this.execStateFrameEvaluate(frame, expr),
             functionName : () => frame.functionName,
             func : () => func,
             localCount : () => this.execStateFrameLocalCount(frame),
             localName : (ix) => this.execStateFrameLocalName(frame, ix),
             localValue: (ix) => this.execStateFrameLocalValue(frame, ix),
             scopeCount : () => frame.scopeChain.length,
             scope : (index) => this.execStateScope(frame.scopeChain[index]),
             allScopes : () => frame.scopeChain.map(
                 this.execStateScope.bind(this))
           };
  }

  // --- Message handlers. -----------------------------------------------------

  dispatchMessage(message) {
    const method = message.method;
    if (method == "Debugger.paused") {
      this.handleDebuggerPaused(message);
    } else if (method == "Debugger.scriptParsed") {
      this.handleDebuggerScriptParsed(message);
    }
  }

  handleDebuggerPaused(message) {
    const params = message.params;

    var debugEvent;
    switch (params.reason) {
      case "exception":
      case "promiseRejection":
        debugEvent = this.DebugEvent.Exception;
        break;
      default:
        // TODO(jgruber): More granularity.
        debugEvent = this.DebugEvent.Break;
        break;
    }

    // Skip break events in this file.
    if (params.callFrames[0].location.scriptId == this.thisScriptId) return;

    // TODO(jgruber): Arguments as needed.
    let execState = { frames : params.callFrames,
                      prepareStep : this.execStatePrepareStep.bind(this),
                      frame : (index) => this.execStateFrame(
                          index ? params.callFrames[index]
                                : params.callFrames[0]),
                      frameCount : () => params.callFrames.length
                    };

    let eventData = this.execStateFrame(params.callFrames[0]);
    if (debugEvent == this.DebugEvent.Exception) {
      eventData.uncaught = () => params.data.uncaught;
    }

    this.invokeListener(debugEvent, execState, eventData);
  }

  handleDebuggerScriptParsed(message) {
    const params = message.params;
    let eventData = { scriptId : params.scriptId,
                      eventType : this.DebugEvent.AfterCompile
                    }

    // TODO(jgruber): Arguments as needed. Still completely missing exec_state,
    // and eventData used to contain the script mirror instead of its id.
    this.invokeListener(this.DebugEvent.AfterCompile, undefined, eventData,
                        undefined);
  }

  invokeListener(event, exec_state, event_data, data) {
    if (this.listener) {
      this.listener(event, exec_state, event_data, data);
    }
  }
}

// Simulate the debug object generated by --expose-debug-as debug.
var debug = { instance : undefined };

Object.defineProperty(debug, 'Debug', { get: function() {
  if (!debug.instance) {
    debug.instance = new DebugWrapper();
    debug.instance.enable();
  }
  return debug.instance;
}});

Object.defineProperty(debug, 'ScopeType', { get: function() {
  const instance = debug.Debug;
  return instance.ScopeType;
}});
