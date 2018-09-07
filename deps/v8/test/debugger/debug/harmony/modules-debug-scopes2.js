// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MODULE


var Debug = debug.Debug;

var test_name;
var listener_delegate;
var listener_called;
var exception;
var begin_test_count = 0;
var end_test_count = 0;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      break_count++;
      listener_called = true;
      listener_delegate(exec_state);
    }
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);


function BeginTest(name) {
  test_name = name;
  listener_delegate = null;
  listener_called = false;
  exception = null;
  begin_test_count++;
}

function EndTest() {
  assertTrue(listener_called, "listener not called for " + test_name);
  assertNull(exception, test_name + " / " + exception);
  end_test_count++;
}


// Check that two scope are the same.
function assertScopeMirrorEquals(scope1, scope2) {
  assertEquals(scope1.scopeType(), scope2.scopeType());
  assertEquals(scope1.frameIndex(), scope2.frameIndex());
  assertEquals(scope1.scopeIndex(), scope2.scopeIndex());
  assertPropertiesEqual(scope1.scopeObject().value(),
                        scope2.scopeObject().value());
}

function CheckFastAllScopes(scopes, exec_state)
{
  var fast_all_scopes = exec_state.frame().allScopes(true);
  var length = fast_all_scopes.length;
  assertTrue(scopes.length >= length);
  for (var i = 0; i < scopes.length && i < length; i++) {
    var scope = fast_all_scopes[length - i - 1];
    assertEquals(scopes[scopes.length - i - 1], scope.scopeType());
  }
}


// Check that the scope chain contains the expected types of scopes.
function CheckScopeChain(scopes, exec_state) {
  var all_scopes = exec_state.frame().allScopes();
  assertEquals(scopes.length, exec_state.frame().scopeCount());
  assertEquals(scopes.length, all_scopes.length, "FrameMirror.allScopes length");
  for (var i = 0; i < scopes.length; i++) {
    var scope = exec_state.frame().scope(i);
    assertEquals(scopes[i], scope.scopeType());
    assertScopeMirrorEquals(all_scopes[i], scope);
  }
  CheckFastAllScopes(scopes, exec_state);
}


function CheckScopeDoesNotHave(properties, number, exec_state) {
  var scope = exec_state.frame().scope(number);
  for (var p of properties) {
    var property_mirror = scope.scopeObject().property(p);
    assertTrue(property_mirror.isUndefined(),
               'property ' + p + ' found in scope');
  }
}


// Check that the scope contains at least minimum_content. For functions just
// check that there is a function.
function CheckScopeContent(minimum_content, number, exec_state) {
  var scope = exec_state.frame().scope(number);
  var minimum_count = 0;
  for (var p in minimum_content) {
    var property_mirror = scope.scopeObject().property(p);
    assertFalse(property_mirror.isUndefined(),
                'property ' + p + ' not found in scope');
    assertEquals(minimum_content[p], property_mirror.value().value(),
                 'property ' + p + ' has unexpected value');
    minimum_count++;
  }

  // 'arguments' and might be exposed in the local and closure scope. Just
  // ignore this.
  var scope_size = scope.scopeObject().properties().length;
  if (!scope.scopeObject().property('arguments').isUndefined()) {
    scope_size--;
  }
  // Ditto for 'this'.
  if (!scope.scopeObject().property('this').isUndefined()) {
    scope_size--;
  }
  // Temporary variables introduced by the parser have not been materialized.
  assertTrue(scope.scopeObject().property('').isUndefined());

  if (scope_size < minimum_count) {
    print('Names found in scope:');
    var names = scope.scopeObject().propertyNames();
    for (var i = 0; i < names.length; i++) {
      print(names[i]);
    }
  }
  assertTrue(scope_size >= minimum_count);
}


////////////////////////////////////////////////////////////////////////////////
// Actual tests.
////////////////////////////////////////////////////////////////////////////////


BeginTest();
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent(
      {exported_var: undefined, imported_var: undefined},
      0, exec_state);
  CheckScopeDoesNotHave(
      ["doesntexist", "exported_let", "imported_let"],
      0, exec_state);
};
debugger;
EndTest();

let local_let = 1;
var local_var = 2;
export let exported_let = 3;
export var exported_var = 4;
import {exported_let as imported_let} from "modules-debug-scopes2.js";
import {exported_var as imported_var} from "modules-debug-scopes2.js";

BeginTest();
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent(
      {exported_let: 3, exported_var: 4,
       imported_let: 3, imported_var: 4}, 0, exec_state);
  CheckScopeDoesNotHave([], 0, exec_state);
};
debugger;
EndTest();

local_let += 10;
local_var += 10;
exported_let += 10;
exported_var += 10;

BeginTest();
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent(
      {exported_let: 13, exported_var: 14,
       imported_let: 13, imported_var: 14}, 0, exec_state);
  CheckScopeDoesNotHave([], 0, exec_state);
};
debugger;
EndTest();
