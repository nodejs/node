// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// LiveEdit feature implementation. The script should be executed after
// debug-debugger.js.

// A LiveEdit namespace is declared inside a single function constructor.
Debug.LiveEdit = new function() {

  // Changes script text and recompiles all relevant functions if possible.
  // The change is always a substring (change_pos, change_pos + change_len)
  // being replaced with a completely different string new_str.
  //
  // Only one function will have its Code changed in result of this function.
  // All nested functions (should they have any instances at the moment) are
  // left unchanged and re-linked to a newly created script instance
  // representing old version of the source. (Generally speaking,
  // during the change all nested functions are erased and completely different
  // set of nested functions are introduced.) All other functions just have
  // their positions updated.
  //
  // @param {Script} script that is being changed
  // @param {Array} change_log a list that collects engineer-readable
  //     description of what happened.
  function ApplyPatch(script, change_pos, change_len, new_str,
      change_log) {

    // Fully compiles source string as a script. Returns Array of
    // FunctionCompileInfo -- a descriptions of all functions of the script.
    // Elements of array are ordered by start positions of functions (from top
    // to bottom) in the source. Fields outer_index and next_sibling_index help
    // to navigate the nesting structure of functions.
    //
    // The script is used for compilation, because it produces code that
    // needs to be linked with some particular script (for nested functions).
    function DebugGatherCompileInfo(source) {
      // Get function info, elements are partially sorted (it is a tree of
      // nested functions serialized as parent followed by serialized children.
      var raw_compile_info = %LiveEditGatherCompileInfo(script, source);
  
      // Sort function infos by start position field.
      var compile_info = new Array();
      var old_index_map = new Array();
      for (var i = 0; i < raw_compile_info.length; i++) {
          compile_info.push(new FunctionCompileInfo(raw_compile_info[i]));
          old_index_map.push(i);
      }
  
      for (var i = 0; i < compile_info.length; i++) {
        var k = i;
        for (var j = i + 1; j < compile_info.length; j++) {
          if (compile_info[k].start_position > compile_info[j].start_position) {
            k = j;
          }
        }
        if (k != i) {
          var temp_info = compile_info[k];
          var temp_index = old_index_map[k];
          compile_info[k] = compile_info[i];
          old_index_map[k] = old_index_map[i];
          compile_info[i] = temp_info;
          old_index_map[i] = temp_index;
        }
      }
  
      // After sorting update outer_inder field using old_index_map. Also
      // set next_sibling_index field.
      var current_index = 0;
  
      // The recursive function, that goes over all children of a particular
      // node (i.e. function info).
      function ResetIndexes(new_parent_index, old_parent_index) {
        var previous_sibling = -1;
        while (current_index < compile_info.length &&
            compile_info[current_index].outer_index == old_parent_index) {
          var saved_index = current_index;
          compile_info[saved_index].outer_index = new_parent_index;
          if (previous_sibling != -1) {
            compile_info[previous_sibling].next_sibling_index = saved_index;
          }
          previous_sibling = saved_index;
          current_index++;
          ResetIndexes(saved_index, old_index_map[saved_index]);
        }
        if (previous_sibling != -1) {
          compile_info[previous_sibling].next_sibling_index = -1;
        }
      }
  
      ResetIndexes(-1, -1);
      Assert(current_index == compile_info.length);
  
      return compile_info;
    }
  
    // Given a positions, finds a function that fully includes the entire
    // change.
    function FindChangedFunction(compile_info, offset, len) {
      // First condition: function should start before the change region.
      // Function #0 (whole-script function) always does, but we want
      // one, that is later in this list.
      var index = 0;
      while (index + 1 < compile_info.length &&
          compile_info[index + 1].start_position <= offset) {
        index++;
      }
      // Now we are at the last function that begins before the change
      // region. The function that covers entire change region is either
      // this function or the enclosing one.
      for (; compile_info[index].end_position < offset + len;
          index = compile_info[index].outer_index) {
        Assert(index != -1);
      }
      return index;
    }
  
    // Variable forward declarations. Preprocessor "Minifier" needs them.
    var old_compile_info;
    var shared_infos;
    // Finds SharedFunctionInfo that corresponds compile info with index
    // in old version of the script.
    function FindFunctionInfo(index) {
      var old_info = old_compile_info[index];
      for (var i = 0; i < shared_infos.length; i++) {
        var info = shared_infos[i];
        if (info.start_position == old_info.start_position &&
            info.end_position == old_info.end_position) {
          return info;
        }
      }
    }
  
    // Replaces function's Code.
    function PatchCode(new_info, shared_info) {
      %LiveEditReplaceFunctionCode(new_info.raw_array, shared_info.raw_array);
  
      change_log.push( {function_patched: new_info.function_name} );
    }
  
    var change_len_old;
    var change_len_new;
    // Translate position in old version of script into position in new
    // version of script.
    function PosTranslator(old_pos) {
      if (old_pos <= change_pos) {
        return old_pos;
      }
      if (old_pos >= change_pos + change_len_old) {
        return old_pos + change_len_new - change_len_old;
      }
      return -1;
    }
  
    var position_change_array;
    var position_patch_report;
    function PatchPositions(new_info, shared_info) {
      if (!shared_info) {
        // TODO(LiveEdit): explain what is happening.
        return;
      }
      var breakpoint_position_update = %LiveEditPatchFunctionPositions(
          shared_info.raw_array, position_change_array);
      for (var i = 0; i < breakpoint_position_update.length; i += 2) {
        var new_pos = breakpoint_position_update[i];
        var break_point_object = breakpoint_position_update[i + 1];
        change_log.push( { breakpoint_position_update:
            { from: break_point_object.source_position(), to: new_pos } } );
        break_point_object.updateSourcePosition(new_pos, script);
      }
      position_patch_report.push( { name: new_info.function_name } );
    }
  
    var link_to_old_script_report;
    var old_script;
    // Makes a function associated with another instance of a script (the
    // one representing its old version). This way the function still
    // may access its own text.
    function LinkToOldScript(shared_info) {
      %LiveEditRelinkFunctionToScript(shared_info.raw_array, old_script);
  
      link_to_old_script_report.push( { name: shared_info.function_name } );
    }
  
  
  
    var old_source = script.source;
    var change_len_old = change_len;
    var change_len_new = new_str.length;
  
    // Prepare new source string.
    var new_source = old_source.substring(0, change_pos) +
        new_str + old_source.substring(change_pos + change_len);
  
    // Find all SharedFunctionInfo's that are compiled from this script.
    var shared_raw_list = %LiveEditFindSharedFunctionInfosForScript(script);
  
    var shared_infos = new Array();
  
    for (var i = 0; i < shared_raw_list.length; i++) {
      shared_infos.push(new SharedInfoWrapper(shared_raw_list[i]));
    }
  
    // Gather compile information about old version of script.
    var old_compile_info = DebugGatherCompileInfo(old_source);
  
    // Gather compile information about new version of script.
    var new_compile_info;
    try {
      new_compile_info = DebugGatherCompileInfo(new_source);
    } catch (e) {
      throw new Failure("Failed to compile new version of script: " + e);
    }
  
    // An index of a single function, that is going to have its code replaced.
    var function_being_patched =
        FindChangedFunction(old_compile_info, change_pos, change_len_old);
  
    // In old and new script versions function with a change should have the
    // same indexes.
    var function_being_patched2 =
        FindChangedFunction(new_compile_info, change_pos, change_len_new);
    Assert(function_being_patched == function_being_patched2,
           "inconsistent old/new compile info");
  
    // Check that function being patched has the same expectations in a new
    // version. Otherwise we cannot safely patch its behavior and should
    // choose the outer function instead.
    while (!CompareFunctionExpectations(
        old_compile_info[function_being_patched],
        new_compile_info[function_being_patched])) {
  
      Assert(old_compile_info[function_being_patched].outer_index ==
          new_compile_info[function_being_patched].outer_index);
      function_being_patched =
          old_compile_info[function_being_patched].outer_index;
      Assert(function_being_patched != -1);
    }
  
    // Check that function being patched is not currently on stack.
    CheckStackActivations(
        [ FindFunctionInfo(function_being_patched) ], change_log );
  
  
    // Committing all changes.
    var old_script_name = CreateNameForOldScript(script);
  
    // Update the script text and create a new script representing an old
    // version of the script.
    var old_script = %LiveEditReplaceScript(script, new_source,
        old_script_name);
  
    PatchCode(new_compile_info[function_being_patched],
        FindFunctionInfo(function_being_patched));
  
    var position_patch_report = new Array();
    change_log.push( {position_patched: position_patch_report} );
  
    var position_change_array = [ change_pos,
                                  change_pos + change_len_old,
                                  change_pos + change_len_new ];
  
    // Update positions of all outer functions (i.e. all functions, that
    // are partially below the function being patched).
    for (var i = new_compile_info[function_being_patched].outer_index;
        i != -1;
        i = new_compile_info[i].outer_index) {
      PatchPositions(new_compile_info[i], FindFunctionInfo(i));
    }
  
    // Update positions of all functions that are fully below the function
    // being patched.
    var old_next_sibling =
        old_compile_info[function_being_patched].next_sibling_index;
    var new_next_sibling =
        new_compile_info[function_being_patched].next_sibling_index;
  
    // We simply go over the tail of both old and new lists. Their tails should
    // have an identical structure.
    if (old_next_sibling == -1) {
      Assert(new_next_sibling == -1);
    } else {
      Assert(old_compile_info.length - old_next_sibling ==
          new_compile_info.length - new_next_sibling);
  
      for (var i = old_next_sibling, j = new_next_sibling;
          i < old_compile_info.length; i++, j++) {
        PatchPositions(new_compile_info[j], FindFunctionInfo(i));
      }
    }
  
    var link_to_old_script_report = new Array();
    change_log.push( { linked_to_old_script: link_to_old_script_report } );
  
    // We need to link to old script all former nested functions.
    for (var i = function_being_patched + 1; i < old_next_sibling; i++) {
      LinkToOldScript(FindFunctionInfo(i), old_script);
    }
  }
  // Function is public.
  this.ApplyPatch = ApplyPatch;
  
  function Assert(condition, message) {
    if (!condition) {
      if (message) {
        throw "Assert " + message;
      } else {
        throw "Assert";
      }
    }
  }
  
  // An object describing function compilation details. Its index fields
  // apply to indexes inside array that stores these objects.
  function FunctionCompileInfo(raw_array) {
    this.function_name = raw_array[0];
    this.start_position = raw_array[1];
    this.end_position = raw_array[2];
    this.param_num = raw_array[3];
    this.code = raw_array[4];
    this.scope_info = raw_array[5];
    this.outer_index = raw_array[6];
    this.next_sibling_index = null;
    this.raw_array = raw_array;
  }
  
  function SharedInfoWrapper(raw_array) {
    this.function_name = raw_array[0];
    this.start_position = raw_array[1];
    this.end_position = raw_array[2];
    this.info = raw_array[3];
    this.raw_array = raw_array;
  }
  
  // Adds a suffix to script name to mark that it is old version.
  function CreateNameForOldScript(script) {
    // TODO(635): try better than this; support several changes.
    return script.name + " (old)";
  }
  
  // Compares a function interface old and new version, whether it
  // changed or not.
  function CompareFunctionExpectations(function_info1, function_info2) {
    // Check that function has the same number of parameters (there may exist
    // an adapter, that won't survive function parameter number change).
    if (function_info1.param_num != function_info2.param_num) {
      return false;
    }
    var scope_info1 = function_info1.scope_info;
    var scope_info2 = function_info2.scope_info;
  
    if (!scope_info1) {
      return !scope_info2;
    }
  
    if (scope_info1.length != scope_info2.length) {
      return false;
    }
  
    // Check that outer scope structure is not changed. Otherwise the function
    // will not properly work with existing scopes.
    return scope_info1.toString() == scope_info2.toString();
  }
  
  // Minifier forward declaration.
  var FunctionPatchabilityStatus;
  
  // For array of wrapped shared function infos checks that none of them
  // have activations on stack (of any thread). Throws a Failure exception
  // if this proves to be false.
  function CheckStackActivations(shared_wrapper_list, change_log) {
    var shared_list = new Array();
    for (var i = 0; i < shared_wrapper_list.length; i++) {
      shared_list[i] = shared_wrapper_list[i].info;
    }
    var result = %LiveEditCheckAndDropActivations(shared_list, true);
    if (result[shared_list.length]) {
      // Extra array element may contain error message.
      throw new Failure(result[shared_list.length]);
    }
  
    var problems = new Array();
    var dropped = new Array();
    for (var i = 0; i < shared_list.length; i++) {
      var shared = shared_wrapper_list[i];
      if (result[i] == FunctionPatchabilityStatus.REPLACED_ON_ACTIVE_STACK) {
        dropped.push({ name: shared.function_name } );
      } else if (result[i] != FunctionPatchabilityStatus.AVAILABLE_FOR_PATCH) {
        var description = {
            name: shared.function_name,
            start_pos: shared.start_position, 
            end_pos: shared.end_position,
            replace_problem:
                FunctionPatchabilityStatus.SymbolName(result[i])
        };
        problems.push(description);
      }
    }
    if (dropped.length > 0) {
      change_log.push({ dropped_from_stack: dropped });
    }
    if (problems.length > 0) {
      change_log.push( { functions_on_stack: problems } );
      throw new Failure("Blocked by functions on stack");
    }
  }
  
  // A copy of the FunctionPatchabilityStatus enum from liveedit.h
  var FunctionPatchabilityStatus = {
      AVAILABLE_FOR_PATCH: 1,
      BLOCKED_ON_ACTIVE_STACK: 2,
      BLOCKED_ON_OTHER_STACK: 3,
      BLOCKED_UNDER_NATIVE_CODE: 4,
      REPLACED_ON_ACTIVE_STACK: 5
  }
  
  FunctionPatchabilityStatus.SymbolName = function(code) {
    var enum = FunctionPatchabilityStatus;
    for (name in enum) {
      if (enum[name] == code) {
        return name;
      }
    }      
  }
  
  
  // A logical failure in liveedit process. This means that change_log
  // is valid and consistent description of what happened.
  function Failure(message) {
    this.message = message;
  }
  // Function (constructor) is public.
  this.Failure = Failure;
  
  Failure.prototype.toString = function() {
    return "LiveEdit Failure: " + this.message;
  }
  
  // A testing entry.
  function GetPcFromSourcePos(func, source_pos) {
    return %GetFunctionCodePositionFromSource(func, source_pos);
  }
  // Function is public.
  this.GetPcFromSourcePos = GetPcFromSourcePos;

  // LiveEdit main entry point: changes a script text to a new string.
  function SetScriptSource(script, new_source, change_log) {
    var old_source = script.source;
    var diff = FindSimpleDiff(old_source, new_source);
    if (!diff) {
      return;
    }
    ApplyPatch(script, diff.change_pos, diff.old_len,
        new_source.substring(diff.change_pos, diff.change_pos + diff.new_len),
        change_log);
  }
  // Function is public.
  this.SetScriptSource = SetScriptSource;
  
  function CompareStringsLinewise(s1, s2) {
    return %LiveEditCompareStringsLinewise(s1, s2);
  }
  // Function is public (for tests).
  this.CompareStringsLinewise = CompareStringsLinewise;

  
  // Finds a difference between 2 strings in form of a single chunk.
  // This is a temporary solution. We should calculate a read diff instead.
  function FindSimpleDiff(old_source, new_source) {
    var change_pos;
    var old_len;
    var new_len;
    
    // A find range block. Whenever control leaves it, it should set 3 local
    // variables declared above.
    find_range:
    {
      // First look from the beginning of strings.
      var pos1;
      {
        var next_pos;
        for (pos1 = 0; true; pos1 = next_pos) {
          if (pos1 >= old_source.length) {
            change_pos = pos1;
            old_len = 0;
            new_len = new_source.length - pos1;
            break find_range;
          }
          if (pos1 >= new_source.length) {
            change_pos = pos1;
            old_len = old_source.length - pos1;
            new_len = 0;
            break find_range;
          }
          if (old_source[pos1] != new_source[pos1]) {
            break;
          }
          next_pos = pos1 + 1;
        }
      }
      // Now compare strings from the ends.
      change_pos = pos1;
      var pos_old;
      var pos_new;
      {
        for (pos_old = old_source.length - 1, pos_new = new_source.length - 1;
            true;
            pos_old--, pos_new--) {
          if (pos_old - change_pos + 1 < 0 || pos_new - change_pos + 1 < 0) {
            old_len = pos_old - change_pos + 2;
            new_len = pos_new - change_pos + 2;
            break find_range;
          }
          if (old_source[pos_old] != new_source[pos_new]) {
            old_len = pos_old - change_pos + 1;
            new_len = pos_new - change_pos + 1;
            break find_range;
          }
        }
      }
    }

    if (old_len == 0 && new_len == 0) {
      // no change
      return;
    }
    
    return { "change_pos": change_pos, "old_len": old_len, "new_len": new_len };
  }
}
