// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*=============================================================================
  This is a convenience script for debugging with WinDbg (akin to gdbinit)
  It can be loaded into WinDbg with: .scriptload full_path\windbg.js

  To printout the help message below into the debugger's command window:
  !help
=============================================================================*/

function help() {
  print("--------------------------------------------------------------------");
  print("  LIVE debugging only");
  print("--------------------------------------------------------------------");
  print("  !jlh(\"local_handle_var_name\")");
  print("      prints object held by the handle");
  print("      e.g. !jlh(\"key\") or !jlh(\"this->receiver_\")");
  print("  !job(address_or_taggedint)");
  print("      prints object at the address, e.g. !job(0x235cb869f9)");
  print("  !jobs(start_address, count)");
  print("      prints 'count' objects from a continuous range of Object pointers");
  print("      e.g. !jobs(0x5f7270, 42)");
  print("  !jst() or !jst");
  print("      prints javascript stack (output goes into the console)");
  print("  !jsbp() or !jsbp");
  print("      sets bp in v8::internal::Execution::Call (begin user's script)");
  print("");
  print("--------------------------------------------------------------------");
  print("  to run any function from this script (live or postmortem):");
  print("");
  print("  dx @$scriptContents.function_name(args)");
  print("      e.g. dx @$scriptContents.pointer_size()");
  print("      e.g. dx @$scriptContents.module_name('v8_test')");
  print("--------------------------------------------------------------------");
}


/*=============================================================================
  Output
=============================================================================*/
function print(s) {
  host.diagnostics.debugLog(s + "\n");
}

function print_filtered(obj, filter) {
  for (let line of obj) {
    if (!filter || line.indexOf(filter) != -1) {
      print(line);
    }
  }
}

function inspect(s) {
  for (var k of Reflect.ownKeys(s)) {
    print(k + " => " + Reflect.get(s, k));
  }
}


/*=============================================================================
  Utils (postmortem and live)
=============================================================================*/
function cast(address, type_name) {
  return host.createTypedObject(address, module_name(), type_name);
}

// Failed to figure out how to get pointer size from the debugger's data model,
// so we parse it out from sizeof(void*) output.
function pointer_size() {
  let ctl = host.namespace.Debugger.Utility.Control;
  let sizeof = ctl.ExecuteCommand("?? sizeof(void*)");
  let output = "";
  for (output of sizeof) {} // unsigned int64 8
  return parseInt(output.trim().split(" ").pop());
}

function poi(address) {
  try {
    // readMemoryValues throws if cannot read from 'address'.
    return host.memory.readMemoryValues(address, 1, pointer_size())[0];
  }
  catch (e){}
}

function get_register(name) {
  return host.namespace.Debugger.State.DebuggerVariables.curthread
         .Registers.User[name];
}

// In debug builds v8 code is compiled into v8.dll, and in release builds
// the code is compiled directly into the executable. If you are debugging some
// other embedder, invoke module_name explicitly from the debugger and provide
// the module name to use.
const known_exes = ["d8", "unittests", "mksnapshot", "chrome", "chromium"];
let module_name_cache;
function module_name(use_this_module) {
  if (use_this_module) {
    module_name_cache = use_this_module;
  }

  if (!module_name_cache) {
    let v8 = host.namespace.Debugger.State.DebuggerVariables.curprocess
             .Modules.Where(
                function(m) {
                 return m.Name.indexOf("\\v8.dll") !== -1;
                });

    if (v8)  {
      module_name_cache = "v8";
    }
    else {
      for (let exe_name in known_exes) {
        let exe = host.namespace.Debugger.State.DebuggerVariables.curprocess
                  .Modules.Where(
                    function(m) {
                      return m.Name.indexOf(`\\${exe_name}.exe`) !== -1;
                    });
        if (exe) {
            module_name_cache = exe_name;
            break;
        }
      }
    }
  }
  return module_name_cache;
};

function make_call(fn) {
  // .call resets current frame to the top one, so have to manually remember
  // and restore it after making the call.
  let curframe = host.namespace.Debugger.State.DebuggerVariables.curframe;
  let ctl = host.namespace.Debugger.Utility.Control;
  let output = ctl.ExecuteCommand(`.call ${fn};g`);
  curframe.SwitchTo();
  return output;
}

// Skips the meta output about the .call invocation.
function make_call_and_print_return(fn) {
  let output = make_call(fn);
  let print_line = false;
  for (let line of output) {
    if (print_line) {
      print(line);
      break;
    }
    if (line.includes(".call returns")) {
      print_line = true;
    }
  }
}


/*=============================================================================
  Wrappers around V8's printing functions and other utils for live-debugging
=============================================================================*/

/*-----------------------------------------------------------------------------
  'address' should be an int (so in hex must include '0x' prefix).
-----------------------------------------------------------------------------*/
function print_object(address) {
  let output = make_call(`_v8_internal_Print_Object(${address})`);

  // skip the first few lines with meta info of .call command
  let skip_line = true;
  for (let line of output) {
    if (!skip_line) {
      print(line);
      continue;
    }
    if (line.includes("deadlocks and corruption of the debuggee")) {
      skip_line = false;
    }
  }
}

/*-----------------------------------------------------------------------------
  'handle_to_object' should be a name of a Handle which can be a local
  variable or it can be a member variable like "this->receiver_".
-----------------------------------------------------------------------------*/
function print_object_from_handle(handle_to_object) {
  let handle = host.evaluateExpression(handle_to_object);
  let location = handle.location_;
  let pobj = poi(location.address);
  print_object(pobj);
}

/*-----------------------------------------------------------------------------
  'start_address' should be an int (so in hex must include '0x' prefix), it can
  point at any continuous memory that contains Object pointers.
-----------------------------------------------------------------------------*/
function print_objects_array(start_address, count) {
  let ctl = host.namespace.Debugger.Utility.Control;
  let psize = pointer_size();
  let addr_int = start_address;
  for (let i = 0; i < count; i++) {
    const addr_hex = `0x${addr_int.toString(16)}`;

    // TODO: Tried using createPointerObject but it throws unknown exception
    // from ChakraCore. Why?
    //let obj = host.createPointerObject(addr_hex, module, "void*");

    let output = ctl.ExecuteCommand(`dp ${addr_hex} l1`);
    let item = "";
    for (item of output) {} // 005f7270  34604101
    let deref = `0x${item.split(" ").pop()}`;
    print(`${addr_hex} -> ${deref}`);
    print_object(deref);

    addr_int += psize;
  }
}

function print_js_stack() {
  make_call("_v8_internal_Print_StackTrace()");
}

function set_user_js_bp() {
  let ctl = host.namespace.Debugger.Utility.Control;
  ctl.ExecuteCommand(`bp ${module_name()}!v8::internal::Execution::Call`)
}

/*=============================================================================
  Initialize short aliased names for the most common commands
=============================================================================*/
function initializeScript() {
  return [
      new host.functionAlias(help, "help"),
      new host.functionAlias(print_object_from_handle, "jlh"),
      new host.functionAlias(print_object, "job"),
      new host.functionAlias(print_objects_array, "jobs"),
      new host.functionAlias(print_js_stack, "jst"),

      new host.functionAlias(set_user_js_bp, "jsbp"),
  ]
}
