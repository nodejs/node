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

// A LiveEdit namespace. It contains functions that modifies JavaScript code
// according to changes of script source (if possible).
//
// When new script source is put in, the difference is calculated textually,
// in form of list of delete/add/change chunks. The functions that include
// change chunk(s) get recompiled, or their enclosing functions are
// recompiled instead.
// If the function may not be recompiled (e.g. it was completely erased in new
// version of the script) it remains unchanged, but the code that could
// create a new instance of this function goes away. An old version of script
// is created to back up this obsolete function.
// All unchanged functions have their positions updated accordingly.
//
// LiveEdit namespace is declared inside a single function constructor.
Debug.LiveEdit = new function() {

  // Applies the change to the script.
  // The change is always a substring (change_pos, change_pos + change_len)
  // being replaced with a completely different string new_str.
  // This API is a legacy and is obsolete.
  //
  // @param {Script} script that is being changed
  // @param {Array} change_log a list that collects engineer-readable
  //     description of what happened.
  function ApplyPatch(script, change_pos, change_len, new_str,
      change_log) {
    var old_source = script.source;
  
    // Prepare new source string.
    var new_source = old_source.substring(0, change_pos) +
        new_str + old_source.substring(change_pos + change_len);
    
    return ApplyPatchMultiChunk(script,
        [ change_pos, change_pos + change_len, change_pos + new_str.length],
        new_source, change_log);
  }
  // Function is public.
  this.ApplyPatch = ApplyPatch;

  // Forward declaration for minifier.
  var FunctionStatus;
  
  // Applies the change to the script.
  // The change is in form of list of chunks encoded in a single array as
  // a series of triplets (pos1_start, pos1_end, pos2_end)
  function ApplyPatchMultiChunk(script, diff_array, new_source, change_log) {

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
      if (shared_info) {
        %LiveEditReplaceFunctionCode(new_info.raw_array, shared_info.raw_array);
        change_log.push( {function_patched: new_info.function_name} );
      } else {
        change_log.push( {function_patched: new_info.function_name,
            function_info_not_found: true} );
      }
  
    }
  
    
    var position_patch_report;
    function PatchPositions(old_info, shared_info) {
      if (!shared_info) {
        // TODO(LiveEdit): function is not compiled yet or is already collected.
        position_patch_report.push( 
            { name: old_info.function_name, info_not_found: true } );
        return;
      }
      var breakpoint_position_update = %LiveEditPatchFunctionPositions(
          shared_info.raw_array, diff_array);
      for (var i = 0; i < breakpoint_position_update.length; i += 2) {
        var new_pos = breakpoint_position_update[i];
        var break_point_object = breakpoint_position_update[i + 1];
        change_log.push( { breakpoint_position_update:
            { from: break_point_object.source_position(), to: new_pos } } );
        break_point_object.updateSourcePosition(new_pos, script);
      }
      position_patch_report.push( { name: old_info.function_name } );
    }
  
    var link_to_old_script_report;
    var old_script;
    // Makes a function associated with another instance of a script (the
    // one representing its old version). This way the function still
    // may access its own text.
    function LinkToOldScript(shared_info, old_info_node) {
      if (shared_info) {
        %LiveEditRelinkFunctionToScript(shared_info.raw_array, old_script);
        link_to_old_script_report.push( { name: shared_info.function_name } );
      } else {
        link_to_old_script_report.push(
            { name: old_info_node.info.function_name, not_found: true } );
      }
    }
    
  
    var old_source = script.source;
   
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
    
    var pos_translator = new PosTranslator(diff_array);

    // Build tree structures for old and new versions of the script.
    var root_old_node = BuildCodeInfoTree(old_compile_info);
    var root_new_node = BuildCodeInfoTree(new_compile_info);

    // Analyze changes.
    MarkChangedFunctions(root_old_node, pos_translator.GetChunks());
    FindCorrespondingFunctions(root_old_node, root_new_node);
    
    // Prepare to-do lists.
    var replace_code_list = new Array();
    var link_to_old_script_list = new Array();
    var update_positions_list = new Array();

    function HarvestTodo(old_node) {
      function CollectDamaged(node) {
        link_to_old_script_list.push(node);
        for (var i = 0; i < node.children.length; i++) {
          CollectDamaged(node.children[i]);
        }
      }
      
      if (old_node.status == FunctionStatus.DAMAGED) {
        CollectDamaged(old_node);
        return;
      }
      if (old_node.status == FunctionStatus.UNCHANGED) {
        update_positions_list.push(old_node);
      } else if (old_node.status == FunctionStatus.SOURCE_CHANGED) {
        update_positions_list.push(old_node);
      } else if (old_node.status == FunctionStatus.CHANGED) {
        replace_code_list.push(old_node);
      }
      for (var i = 0; i < old_node.children.length; i++) {
        HarvestTodo(old_node.children[i]);
      }
    }

    HarvestTodo(root_old_node);
    
    // Collect shared infos for functions whose code need to be patched.
    var replaced_function_infos = new Array();
    for (var i = 0; i < replace_code_list.length; i++) {
      var info = FindFunctionInfo(replace_code_list[i].array_index);
      if (info) {
        replaced_function_infos.push(info);
      }
    }

    // Check that function being patched is not currently on stack.
    CheckStackActivations(replaced_function_infos, change_log);
  
  
    // We haven't changed anything before this line yet.
    // Committing all changes.

    // Create old script if there are function linked to old version.
    if (link_to_old_script_list.length > 0) {
      var old_script_name = CreateNameForOldScript(script);
      
      // Update the script text and create a new script representing an old
      // version of the script.
      var old_script = %LiveEditReplaceScript(script, new_source,
          old_script_name);
      
      var link_to_old_script_report = new Array();
      change_log.push( { linked_to_old_script: link_to_old_script_report } );
    
      // We need to link to old script all former nested functions.
      for (var i = 0; i < link_to_old_script_list.length; i++) {
        LinkToOldScript(
            FindFunctionInfo(link_to_old_script_list[i].array_index),
            link_to_old_script_list[i]);
      }
    }
    

    for (var i = 0; i < replace_code_list.length; i++) {
      PatchCode(replace_code_list[i].corresponding_node.info,
          FindFunctionInfo(replace_code_list[i].array_index));
    }
  
    var position_patch_report = new Array();
    change_log.push( {position_patched: position_patch_report} );
    
    for (var i = 0; i < update_positions_list.length; i++) {
      // TODO(LiveEdit): take into account wether it's source_changed or
      // unchanged and whether positions changed at all.
      PatchPositions(update_positions_list[i].info,
          FindFunctionInfo(update_positions_list[i].array_index));
    }
  }
  // Function is public.
  this.ApplyPatchMultiChunk = ApplyPatchMultiChunk;

  
  function Assert(condition, message) {
    if (!condition) {
      if (message) {
        throw "Assert " + message;
      } else {
        throw "Assert";
      }
    }
  }

  function DiffChunk(pos1, pos2, len1, len2) {
    this.pos1 = pos1;
    this.pos2 = pos2;
    this.len1 = len1;
    this.len2 = len2;
  }
  
  function PosTranslator(diff_array) {
    var chunks = new Array();
    var pos1 = 0;
    var pos2 = 0;
    for (var i = 0; i < diff_array.length; i += 3) {
      pos2 += diff_array[i] - pos1 + pos2;
      pos1 = diff_array[i];
      chunks.push(new DiffChunk(pos1, pos2, diff_array[i + 1] - pos1,
          diff_array[i + 2] - pos2));
      pos1 = diff_array[i + 1];
      pos2 = diff_array[i + 2];
    }
    this.chunks = chunks;
  }
  PosTranslator.prototype.GetChunks = function() {
    return this.chunks;
  }
  
  PosTranslator.prototype.Translate = function(pos, inside_chunk_handler) {
    var array = this.chunks; 
    if (array.length == 0 || pos < array[0]) {
      return pos;
    }
    var chunk_index1 = 0;
    var chunk_index2 = array.length - 1;

    while (chunk_index1 < chunk_index2) {
      var middle_index = (chunk_index1 + chunk_index2) / 2;
      if (pos < array[middle_index + 1].pos1) {
        chunk_index2 = middle_index;
      } else {
        chunk_index1 = middle_index + 1;
      }
    }
    var chunk = array[chunk_index1];
    if (pos >= chunk.pos1 + chunk.len1) {
      return pos += chunk.pos2 + chunk.len2 - chunk.pos1 - chunk.len1; 
    }
    
    if (!inside_chunk_handler) {
      inside_chunk_handler = PosTranslator.default_inside_chunk_handler;
    }
    inside_chunk_handler(pos, chunk);
  }

  PosTranslator.default_inside_chunk_handler = function() {
    Assert(false, "Cannot translate position in chaged area");
  }
  
  var FunctionStatus = {
      // No change to function or its inner functions; however its positions
      // in script may have been shifted. 
      UNCHANGED: "unchanged",
      // The code of a function remains unchanged, but something happened inside
      // some inner functions.
      SOURCE_CHANGED: "source changed",
      // The code of a function is changed or some nested function cannot be
      // properly patched so this function must be recompiled.
      CHANGED: "changed",
      // Function is changed but cannot be patched.
      DAMAGED: "damaged"
  }
  
  function CodeInfoTreeNode(code_info, children, array_index) {
    this.info = code_info;
    this.children = children;
    // an index in array of compile_info
    this.array_index = array_index; 
    this.parent = void(0);
    
    this.status = FunctionStatus.UNCHANGED;
    // Status explanation is used for debugging purposes and will be shown
    // in user UI if some explanations are needed.
    this.status_explanation = void(0);
    this.new_start_pos = void(0);
    this.new_end_pos = void(0);
    this.corresponding_node = void(0);
  }
  
  // From array of function infos that is implicitly a tree creates
  // an actual tree of functions in script.
  function BuildCodeInfoTree(code_info_array) {
    // Throughtout all function we iterate over input array.
    var index = 0;

    // Recursive function that builds a branch of tree. 
    function BuildNode() {
      var my_index = index;
      index++;
      var child_array = new Array();
      while (index < code_info_array.length &&
          code_info_array[index].outer_index == my_index) {
        child_array.push(BuildNode());
      }
      var node = new CodeInfoTreeNode(code_info_array[my_index], child_array,
          my_index);
      for (var i = 0; i < child_array.length; i++) {
        child_array[i].parent = node;
      }
      return node;
    }
    
    var root = BuildNode();
    Assert(index == code_info_array.length);
    return root;
  }

  // Applies a list of the textual diff chunks onto the tree of functions.
  // Determines status of each function (from unchanged to damaged). However
  // children of unchanged functions are ignored.
  function MarkChangedFunctions(code_info_tree, chunks) {

    // A convenient interator over diff chunks that also translates
    // positions from old to new in a current non-changed part of script.
    var chunk_it = new function() {
      var chunk_index = 0;
      var pos_diff = 0;
      this.current = function() { return chunks[chunk_index]; }
      this.next = function() {
        var chunk = chunks[chunk_index];
        pos_diff = chunk.pos2 + chunk.len2 - (chunk.pos1 + chunk.len1); 
        chunk_index++;
      }
      this.done = function() { return chunk_index >= chunks.length; }
      this.TranslatePos = function(pos) { return pos + pos_diff; }
    };

    // A recursive function that processes internals of a function and all its
    // inner functions. Iterator chunk_it initially points to a chunk that is
    // below function start.
    function ProcessInternals(info_node) {
      info_node.new_start_pos = chunk_it.TranslatePos(
          info_node.info.start_position); 
      var child_index = 0;
      var code_changed = false;
      var source_changed = false;
      // Simultaneously iterates over child functions and over chunks.
      while (!chunk_it.done() &&
          chunk_it.current().pos1 < info_node.info.end_position) {
        if (child_index < info_node.children.length) {
          var child = info_node.children[child_index];
          
          if (child.info.end_position <= chunk_it.current().pos1) {
            ProcessUnchangedChild(child);
            child_index++;
            continue;
          } else if (child.info.start_position >=
              chunk_it.current().pos1 + chunk_it.current().len1) {
            code_changed = true;
            chunk_it.next();
            continue;
          } else if (child.info.start_position <= chunk_it.current().pos1 &&
              child.info.end_position >= chunk_it.current().pos1 +
              chunk_it.current().len1) {
            ProcessInternals(child);
            source_changed = source_changed ||
                ( child.status != FunctionStatus.UNCHANGED );
            code_changed = code_changed ||
                ( child.status == FunctionStatus.DAMAGED );
            child_index++;
            continue;
          } else {
            code_changed = true;
            child.status = FunctionStatus.DAMAGED;
            child.status_explanation =
                "Text diff overlaps with function boundary";
            child_index++;
            continue;
          }
        } else {
          if (chunk_it.current().pos1 + chunk_it.current().len1 <= 
              info_node.info.end_position) {
            info_node.status = FunctionStatus.CHANGED;
            chunk_it.next();
            continue;
          } else {
            info_node.status = FunctionStatus.DAMAGED;
            info_node.status_explanation =
                "Text diff overlaps with function boundary";
            return;
          }
        }
        Assert("Unreachable", false);
      }
      while (child_index < info_node.children.length) {
        var child = info_node.children[child_index];
        ProcessUnchangedChild(child);
        child_index++;
      }
      if (code_changed) {
        info_node.status = FunctionStatus.CHANGED;
      } else if (source_changed) {
        info_node.status = FunctionStatus.SOURCE_CHANGED;
      }
      info_node.new_end_pos =
          chunk_it.TranslatePos(info_node.info.end_position); 
    }
    
    function ProcessUnchangedChild(node) {
      node.new_start_pos = chunk_it.TranslatePos(node.info.start_position);
      node.new_end_pos = chunk_it.TranslatePos(node.info.end_position);
    }
    
    ProcessInternals(code_info_tree);
  }

  // For ecah old function (if it is not damaged) tries to find a corresponding
  // function in new script. Typically it should succeed (non-damaged functions
  // by definition may only have changes inside their bodies). However there are
  // reasons for corresponence not to be found; function with unmodified text
  // in new script may become enclosed into other function; the innocent change
  // inside function body may in fact be something like "} function B() {" that
  // splits a function into 2 functions.
  function FindCorrespondingFunctions(old_code_tree, new_code_tree) {

    // A recursive function that tries to find a correspondence for all
    // child functions and for their inner functions.
    function ProcessChildren(old_node, new_node) {
      var old_children = old_node.children;
      var new_children = new_node.children;

      var old_index = 0;
      var new_index = 0;
      while (old_index < old_children.length) {
        if (old_children[old_index].status == FunctionStatus.DAMAGED) {
          old_index++;
        } else if (new_index < new_children.length) {
          if (new_children[new_index].info.start_position <
              old_children[old_index].new_start_pos) {
            new_index++;
          } else if (new_children[new_index].info.start_position ==
              old_children[old_index].new_start_pos) {
            if (new_children[new_index].info.end_position ==
                old_children[old_index].new_end_pos) {
              old_children[old_index].corresponding_node =
                  new_children[new_index];
              if (old_children[old_index].status != FunctionStatus.UNCHANGED) {
                ProcessChildren(old_children[old_index],
                    new_children[new_index]);
                if (old_children[old_index].status == FunctionStatus.DAMAGED) {
                  old_node.status = FunctionStatus.CHANGED;
                }
              }
            } else {
              old_children[old_index].status = FunctionStatus.DAMAGED;
              old_children[old_index].status_explanation =
                  "No corresponding function in new script found";
              old_node.status = FunctionStatus.CHANGED;
            }
            new_index++;
            old_index++;
          } else {
            old_children[old_index].status = FunctionStatus.DAMAGED;
            old_children[old_index].status_explanation =
                "No corresponding function in new script found";
            old_node.status = FunctionStatus.CHANGED;
            old_index++;
          }
        } else {
          old_children[old_index].status = FunctionStatus.DAMAGED;
          old_children[old_index].status_explanation =
              "No corresponding function in new script found";
          old_node.status = FunctionStatus.CHANGED;
          old_index++;
        }
      }
      
      if (old_node.status == FunctionStatus.CHANGED) {
        if (!CompareFunctionExpectations(old_node.info, new_node.info)) {
          old_node.status = FunctionStatus.DAMAGED;
          old_node.status_explanation = "Changed code expectations";
        }
      }
    }

    ProcessChildren(old_code_tree, new_code_tree);
    
    old_code_tree.corresponding_node = new_code_tree;
    Assert(old_code_tree.status != FunctionStatus.DAMAGED,
        "Script became damaged");
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
    var diff = CompareStringsLinewise(old_source, new_source);
    if (diff.length == 0) {
      change_log.push( { empty_diff: true } );
      return;
    }
    ApplyPatchMultiChunk(script, diff, new_source, change_log);
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
