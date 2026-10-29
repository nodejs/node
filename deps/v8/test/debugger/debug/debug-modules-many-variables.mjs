// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --no-turbofan

// setVariableValue / scope inspection on a module scope with more module
// variables than kScopeInfoMaxInlinedLocalNamesSize, so the debugger's
// ScopeInfo::ModuleIndex lookups go through the module-variables hashtable.

var Debug = debug.Debug;

export let v0 = 0;
export let v1 = 1;
export let v2 = 2;
export let v3 = 3;
export let v4 = 4;
export let v5 = 5;
export let v6 = 6;
export let v7 = 7;
export let v8 = 8;
export let v9 = 9;
export let v10 = 10;
export let v11 = 11;
export let v12 = 12;
export let v13 = 13;
export let v14 = 14;
export let v15 = 15;
export let v16 = 16;
export let v17 = 17;
export let v18 = 18;
export let v19 = 19;
export let v20 = 20;
export let v21 = 21;
export let v22 = 22;
export let v23 = 23;
export let v24 = 24;
export let v25 = 25;
export let v26 = 26;
export let v27 = 27;
export let v28 = 28;
export let v29 = 29;
export let v30 = 30;
export let v31 = 31;
export let v32 = 32;
export let v33 = 33;
export let v34 = 34;
export let v35 = 35;
export let v36 = 36;
export let v37 = 37;
export let v38 = 38;
export let v39 = 39;
export let v40 = 40;
export let v41 = 41;
export let v42 = 42;
export let v43 = 43;
export let v44 = 44;
export let v45 = 45;
export let v46 = 46;
export let v47 = 47;
export let v48 = 48;
export let v49 = 49;
export let v50 = 50;
export let v51 = 51;
export let v52 = 52;
export let v53 = 53;
export let v54 = 54;
export let v55 = 55;
export let v56 = 56;
export let v57 = 57;
export let v58 = 58;
export let v59 = 59;
export let v60 = 60;
export let v61 = 61;
export let v62 = 62;
export let v63 = 63;
export let v64 = 64;
export let v65 = 65;
export let v66 = 66;
export let v67 = 67;
export let v68 = 68;
export let v69 = 69;
export let v70 = 70;
export let v71 = 71;
export let v72 = 72;
export let v73 = 73;
export let v74 = 74;
export let v75 = 75;
export let v76 = 76;
export let v77 = 77;
export let v78 = 78;
export let v79 = 79;
export let v80 = 80;
export let v81 = 81;
export let v82 = 82;
export let v83 = 83;
export let v84 = 84;
export let v85 = 85;
export let v86 = 86;
export let v87 = 87;
export let v88 = 88;
export let v89 = 89;
export let v90 = 90;
export let v91 = 91;
export let v92 = 92;
export let v93 = 93;
export let v94 = 94;
export let v95 = 95;
export let v96 = 96;
export let v97 = 97;
export let v98 = 98;
export let v99 = 99;
export let v100 = 100;
export let v101 = 101;
export let v102 = 102;
export let v103 = 103;
export let v104 = 104;
export let v105 = 105;
export let v106 = 106;
export let v107 = 107;
export let v108 = 108;
export let v109 = 109;
export let v110 = 110;
export let v111 = 111;
export let v112 = 112;
export let v113 = 113;
export let v114 = 114;
export let v115 = 115;
export let v116 = 116;
export let v117 = 117;
export let v118 = 118;
export let v119 = 119;

let notExported = 'a';

{
  let exception;
  function listener(event, exec_state) {
    if (event != Debug.DebugEvent.Break) return;
    try {
      let module_scope = exec_state.frame().scope(1);
      assertEquals(debug.ScopeType.Module, module_scope.scopeType());
      // Existing variables at various positions in the module_variables list.
      module_scope.setVariableValue('v0', 1000);
      module_scope.setVariableValue('v74', 1074);
      module_scope.setVariableValue('v75', 1075);
      module_scope.setVariableValue('v119', 2000);
      module_scope.setVariableValue('notExported', 'b');
      // Non-existing variable must throw.
      assertThrows(() => module_scope.setVariableValue('spargel', 42));
    } catch (e) { exception = e; }
  }
  Debug.setListener(listener);
  debugger;
  Debug.setListener(null);
  assertEquals(undefined, exception);
  assertEquals(1000, v0);
  assertEquals(1074, v74);
  assertEquals(1075, v75);
  assertEquals(2000, v119);
  assertEquals('b', notExported);
  assertEquals(1, v1);
}
