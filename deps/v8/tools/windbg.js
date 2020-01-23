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
  if (supports_call_command()) {
  print("--------------------------------------------------------------------");
  print("  LIVE debugging only");
  print("--------------------------------------------------------------------");
  print("  !jlh(\"local_handle_var_name\")");
  print("      prints object held by the handle");
  print("      e.g. !jlh(\"key\") or !jlh(\"this->receiver_\")");
  print("  !job(address_or_taggedint)");
  print("      prints object at the address, e.g. !job(0x235cb869f9)");
  print("  !jst() or !jst");
  print("      prints javascript stack (output goes into the console)");
  print("  !jsbp() or !jsbp");
  print("      sets bp in v8::internal::Execution::Call");
  print("");
  }

  print("--------------------------------------------------------------------");
  print("  Setup of the script");
  print("--------------------------------------------------------------------");
  print("  !set_module(\"module_name_no_extension\")");
  print("      we'll try the usual suspects for where v8's code might have");
  print("      been linked into, but you can also set it manually,");
  print("      e.g. !set_module(\"v8_for_testing\")");
  print("  !set_iso(isolate_address)");
  print("      call this function before using !mem or other heap routines");
  print("");

  print("--------------------------------------------------------------------");
  print("  Managed heap");
  print("--------------------------------------------------------------------");
  print("  !mem or !mem(\"space1[ space2 ...]\")");
  print("      prints memory chunks from the 'space' owned by the heap in the");
  print("      isolate set by !set_iso; valid values for 'space' are:");
  print("      new, old, map, code, lo [large], nlo [newlarge], ro [readonly]");
  print("      if no 'space' specified prints memory chunks for all spaces,");
  print("      e.g. !mem(\"code\"), !mem(\"ro new old\")");
  print("  !where(address)");
  print("      prints name of the space and address of the MemoryChunk the");
  print("      'address' is from, e.g. !where(0x235cb869f9)");
  print("  !rs(chunk_address, set_id = 0)");
  print("      prints slots from the remembered set in the MemoryChunk. If");
  print("      'chunk_address' isn't specified, prints for all chunks in the");
  print("      old space; 'set_id' should match RememberedSetType enum,");
  print("      e.g. !rs, !rs 0x2fb14780000, !rs(0x2fb14780000, 1)");
  print("");

  print("--------------------------------------------------------------------");
  print("  Managed objects");
  print("--------------------------------------------------------------------");
  print("  !jot(tagged_addr, depth)");
  print("      dumps the tree of objects using 'tagged_addr' as a root,");
  print("      assumes that pointer fields are aligned at ptr_size boundary,");
  print("      unspecified depth means 'unlimited',");
  print("      e.g. !jot(0x235cb869f9, 2), !jot 0x235cb869f9");
  print("  !jo_in_range(start_addr, end_addr)");
  print("      prints address/map pointers of objects found inside the range");
  print("      specified by 'start_addr' and 'end_addr', assumes the object");
  print("      pointers to be aligned at ptr_size boundary,");
  print("      e.g. !jo_in_range(0x235cb869f8 - 0x100, 0x235cb869f8 + 0x1a0");
  print("  !jo_prev(address, max_slots = 100)");
  print("      prints address and map pointer of the nearest object within");
  print("      'max_slots' before the given 'address', assumes the object");
  print("      pointers to be aligned at ptr_size boundary,");
  print("      e.g. !jo_prev 0x235cb869f8, !jo_prev(0x235cb869f9, 16)");
  print("  !jo_next(address, max_slots = 100)");
  print("      prints address and map pointer of the nearest object within");
  print("      'max_slots' following the given 'address', assumes the object");
  print("      pointers to be aligned at ptr_size boundary,");
  print("      e.g. !jo_next 0x235cb869f8, !jo_next(0x235cb869f9, 20)");
  print("");

  print("--------------------------------------------------------------------");
  print("  Miscellaneous");
  print("--------------------------------------------------------------------");
  print("  !dp(address, count = 10)");
  print("      similar to the built-in 'dp' command but augments output with");
  print("      more data for values that are managed pointers, note that it");
  print("      aligns the given 'address' at ptr_sized boundary,");
  print("      e.g. !dp 0x235cb869f9, !dp(0x235cb869f9, 500), !dp @rsp");
  print("  !handles(print_handles = false)");
  print("      prints stats for handles, if 'print_handles' is true will");
  print("      output all handles as well,");
  print("      e.g. !handles, !handles(), !handles(true)");
  print("");

  print("--------------------------------------------------------------------");
  print("  To run any function from this script (live or postmortem):");
  print("");
  print("  dx @$scriptContents.function_name(args)");
  print("      e.g. dx @$scriptContents.pointer_size()");
  print("      e.g. dx @$scriptContents.is_map(0x235cb869f9)");
  print("--------------------------------------------------------------------");
}

/*=============================================================================
  On scrip load
=============================================================================*/

/*=============================================================================
  Output
=============================================================================*/
function print(s) {
  host.diagnostics.debugLog(s + "\n");
}

function inspect(s) {
  for (let k of Reflect.ownKeys(s)) {
    // Attempting to print either of:
    // 'Reflect.get(s, k)', 'typeof Reflect.get(s, k)', 's[k]'
    // might throw: "Error: Object does not have a size",
    // while 'typeof s[k]' returns 'undefined' and prints the full list of
    // properties. Oh well...
    print(`${k} => ${typeof s[k]}`);
  }
}

function hex(number) {
  return `0x${number.toString(16)}`;
}

/*=============================================================================
  Utils (postmortem and live)
=============================================================================*/
// WinDbg wraps large integers (0x80000000+) into an object of library type that
// fails isInteger test (and, consequently fail isSafeInteger test even if the
// original value was a safe integer).
// However, that library type does have a set of methods on it which you can use
// to force conversion:
// .asNumber() / .valueOf(): Performs conversion to JavaScript number.
// Throws if the ordinal part of the 64-bit number does not pack into JavaScript
// number without loss of precision.
// .convertToNumber(): Performs conversion to JavaScript number.
// Does NOT throw if the ordinal part of the 64-bit number does not pack into
// JavaScript number. This will simply result in loss of precision.
// The library will also add these methods to the prototype for the standard
// number prototype. Meaning you can always .asNumber() / .convertToNumber() to
// get either JavaScript number or the private Int64 type into a JavaScript
// number.
// We could use the conversion functions but it seems that doing the conversion
// via toString is just as good and slightly more generic...
function int(val) {
  if (typeof val === 'number') {
    return Number.isInteger(val) ? val : undefined;
  }
  if (typeof val === 'object') {
    let n = parseInt(val.toString());
    return isNaN(n) ? undefined : n;
  }
  return undefined;
}

function is_live_session() {
  // Assume that there is a single session (not sure how to get multiple ones
  // going, maybe, in kernel debugging?).
  return (host.namespace.Debugger.Sessions[0].Attributes.Target.IsLiveTarget);
}

function is_TTD_session() {
  // Assume that there is a single session (not sure how to get multiple ones
  // going, maybe, in kernel debugging?).
  return (host.namespace.Debugger.Sessions[0].Attributes.Target.IsTTDTarget);
}

function supports_call_command() {
  return is_live_session() && !is_TTD_session();
}

function cast(address, type_name) {
  return host.createTypedObject(address, module_name(), type_name);
}

function pointer_size() {
  return host.namespace.Debugger.Sessions[0].Attributes.Machine.PointerSize;
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

// JS doesn't do bitwise operations on large integers, so let's do it ourselves
// using hex string representation.
function bitwise_and(l, r) {
  l = hex(l);
  let l_length = l.length;
  r = hex(r);
  let r_length = r.length;
  let res = "";
  let length = Math.min(l_length, r_length) - 2;  // to account for "0x"
  for (let i = 1; i <= length; i++) {
    res = (parseInt(l[l_length - i], 16) & parseInt(r[r_length - i], 16))
          .toString(16) + res;
  }
  return parseInt(res, 16);
}


/*=============================================================================
  Script setup
=============================================================================*/
// In debug builds v8 code is compiled into v8.dll, and in release builds
// the code is compiled directly into the executable. If you are debugging some
// other embedder, run !set_module and provide the module name to use.
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

    let v8_test = host.namespace.Debugger.State.DebuggerVariables.curprocess
                  .Modules.Where(
                      function(m) {
                      return m.Name.indexOf("\\v8_for_testing.dll") !== -1;
                      });

    if (v8.Count() > 0) {
      module_name_cache = "v8";
    }
    else if (v8_test.Count() > 0) {
      module_name_cache = "v8_for_testing";
    }
    else {
      for (let exe_name in known_exes) {
        let exe = host.namespace.Debugger.State.DebuggerVariables.curprocess
                  .Modules.Where(
                    function(m) {
                      return m.Name.indexOf(`\\${exe_name}.exe`) !== -1;
                    });
        if (exe.Count() > 0) {
            module_name_cache = exe_name;
            break;
        }
      }
    }
  }

  if (!module_name_cache) {
    print(`ERROR. Couldn't determine module name for v8's symbols.`);
    print(`Please run !set_module (e.g. "!set_module \"v8_for_testing\"")`);
  }
  return module_name_cache;
};

let using_ptr_compr = false;
let isolate_address = 0;
function set_isolate_address(addr, ptr_compr) {
  isolate_address = addr;

  if (typeof ptr_compr === 'undefined') {
    ptr_compr = (bitwise_and(isolate_address, 0xffffffff) == 0);
  }
  using_ptr_compr = ptr_compr;

  if (using_ptr_compr) {
    print("The target is using pointer compression.");
  }
}


/*=============================================================================
  Wrappers around V8's printing functions and other utils for live-debugging
=============================================================================*/
function make_call(fn) {
  if (!supports_call_command()) {
    print("ERROR: This command is supported in live sessions only!");
    return;
  }

  // .call resets current frame to the top one, so have to manually remember
  // and restore it after making the call.
  let curframe = host.namespace.Debugger.State.DebuggerVariables.curframe;
  let ctl = host.namespace.Debugger.Utility.Control;
  let output = ctl.ExecuteCommand(`.call ${fn};g`);
  curframe.SwitchTo();
  return output;
}

function print_object(address) {
  let output = make_call(`_v8_internal_Print_Object(${decomp(address)})`);

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

function print_object_from_handle(handle_to_object) {
  let handle = host.evaluateExpression(handle_to_object);
  let location = handle.location_;
  let pobj = poi(location.address);  // handles use uncompressed pointers
  print_object(pobj);
}

function print_js_stack() {
  make_call("_v8_internal_Print_StackTrace()");
}

function set_user_js_bp() {
  let ctl = host.namespace.Debugger.Utility.Control;
  ctl.ExecuteCommand(`bp ${module_name()}!v8::internal::Execution::Call`)
}


/*=============================================================================
  Managed heap related functions (live and post-mortem debugging)
=============================================================================*/
/*-----------------------------------------------------------------------------
    Pointer compression
-----------------------------------------------------------------------------*/
function tagged_size() {
  return using_ptr_compr ? 4 : pointer_size();
}

function get_compressed_ptr_base() {
  if (!using_ptr_compr) return 0;

  return isolate_address;
}

function decomp(value) {
  if (value > 0xffffffff) return value;
  return get_compressed_ptr_base() + value;
}

// Adjust for possible pointer compression ('address' is assumed to be on the
// managed heap).
function poim(address) {
  try {
    // readMemoryValues throws if cannot read from 'address'.
    return host.memory.readMemoryValues(decomp(address), 1, tagged_size())[0];
  }
  catch (e){}
}

/*-----------------------------------------------------------------------------
    Exploring objects
-----------------------------------------------------------------------------*/
function is_map(addr) {
  let address = int(addr);
  if (!Number.isSafeInteger(address) || address % 2 == 0) return false;

  // the first field in all objects, including maps, is a map pointer, but for
  // maps the pointer is always the same - the meta map that points to itself.
  const map_addr = int(poim(address - 1));
  if (!Number.isSafeInteger(map_addr)) return false;

  const map_map_addr = int(poim(map_addr - 1));
  if (!Number.isSafeInteger(map_map_addr)) return false;

  return (map_addr === map_map_addr);
}

function is_likely_object(addr) {
  let address = int(addr);
  if (!Number.isSafeInteger(address) || address % 2 == 0) return false;

  // the first field in all objects must be a map pointer
  return is_map(poim(address - 1));
}

function find_object_near(aligned_addr, max_distance, step_op) {
  if (!step_op) {
    const step = tagged_size();
    const prev =
      find_object_near(aligned_addr, max_distance, x => x - step);
    const next =
      find_object_near(aligned_addr, max_distance, x => x + step);

    if (!prev) return next;
    if (!next) return prev;
    return (addr - prev <= next - addr) ? prev : next;
  }

  let maybe_map_addr = poim(aligned_addr);
  let iters = 0;
  while (maybe_map_addr && iters < max_distance) {
    if (is_map(maybe_map_addr)) {
      return aligned_addr;
    }
    aligned_addr = step_op(aligned_addr);
    maybe_map_addr = poim(aligned_addr);
    iters++;
  }
}

function find_object_prev(addr, max_distance) {
  if (!Number.isSafeInteger(int(addr))) return;

  const ptr_size = tagged_size();
  const aligned_addr = addr - (addr % ptr_size);
  return find_object_near(aligned_addr, max_distance, x => x - ptr_size);
}

function find_object_next(addr, max_distance) {
  if (!Number.isSafeInteger(int(addr))) return;

  const ptr_size = tagged_size();
  const aligned_addr = addr - (addr % ptr_size) + ptr_size;
  return find_object_near(aligned_addr, max_distance, x => x + ptr_size);
}

function print_object_prev(addr, max_slots = 100) {
  let obj_addr = find_object_prev(addr, max_slots);
  if (!obj_addr) {
    print(
      `No object found within ${max_slots} slots prior to ${hex(addr)}`);
  }
  else {
    print(
      `found object: ${hex(obj_addr + 1)} : ${hex(poim(obj_addr))}`);
  }
}

function print_object_next(addr, max_slots = 100) {
  let obj_addr = find_object_next(addr, max_slots);
  if (!obj_addr) {
    print(
      `No object found within ${max_slots} slots following ${hex(addr)}`);
  }
  else {
    print(
      `found object: ${hex(obj_addr + 1)} : ${hex(poim(obj_addr))}`);
  }
}

// This function assumes that pointers to objects are stored at ptr-size aligned
// boundaries.
function print_objects_in_range(start, end){
  if (!Number.isSafeInteger(int(start)) || !Number.isSafeInteger(int(end))) {
    return;
  }
  const ptr_size = pointer_size();
  if (start < ptr_size || end <= start) return;

  let iters = (end - start) / ptr_size;
  let cur = start - ptr_size;
  print(`===============================================`);
  print(`objects in range ${hex(start)} - ${hex(end)}`);
  print(`===============================================`);
  let count = 0;
  while (cur && cur < end) {
    let obj = find_object_next(cur, iters);
    if (obj) {
      count++;
      print(`${hex(obj + 1)} : ${hex(poim(obj))}`);
      iters  = (end - cur) / ptr_size;
    }
    cur = obj + ptr_size;
  }
  print(`===============================================`);
  print(`found ${count} objects in range ${hex(start)} - ${hex(end)}`)
  print(`===============================================`);
}

// This function assumes the pointer fields to be ptr-size aligned.
function print_objects_tree(root, depth_limit) {
  if(!is_likely_object(root)) {
    print(`${hex(root)} doesn't look like an object`);
    return;
  }

  let path = [];

  function impl(obj, depth, depth_limit) {
    const ptr_size = tagged_size();
    // print the current object and its map pointer
    const this_obj =
      `${" ".repeat(2 * depth)}${hex(obj)} : ${hex(poim(obj - 1))}`;
    const cutoff = depth_limit && depth == depth_limit - 1;
    print(`${this_obj}${cutoff ? " (...)" : ""}`);
    if (cutoff) return;

    path[depth] = obj;
    path.length = depth + 1;
    let cur = obj - 1 + ptr_size;

    // Scan downwards until an address that is likely to be at the start of
    // another object, in which case it's time to pop out from the recursion.
    let iter = 0; // an arbitrary guard to avoid hanging the debugger
    let seen = new Set(path);
    while (!is_likely_object(cur + 1) && iter < 100) {
      iter++;
      let field = poim(cur);
      if (is_likely_object(field)) {
        if (seen.has(field)) {
          print(
            `${" ".repeat(2 * depth + 2)}cycle: ${hex(cur)}->${hex(field)}`);
        }
        else {
          impl(field, depth + 1, depth_limit);
        }
      }
      cur += ptr_size;
    }
  }
  print(`===============================================`);
  impl(root, 0, depth_limit);
  print(`===============================================`);
}

/*-----------------------------------------------------------------------------
    Memory spaces
-----------------------------------------------------------------------------*/
const NEVER_EVACUATE = 1 << 7; // see src\heap\spaces.h

function print_memory_chunk_list(space_type, front, top, age_mark) {
  let alloc_pos = top ? ` (allocating at: ${top})` : "";
  let age_mark_pos = age_mark ? ` (age_mark at: ${top})` : "";
  print(`${space_type}${alloc_pos}${age_mark_pos}:`);
  if (front.isNull) {
    print("<empty>\n");
    return;
  }

  let cur = front;
  while (!cur.isNull) {
    let imm = cur.flags_ & NEVER_EVACUATE ? "*" : " ";
    let addr = hex(cur.address);
    let area = `${hex(cur.area_start_)} - ${hex(cur.area_end_)}`;
    let dt = `dt ${addr} ${module_name()}!v8::internal::MemoryChunk`;
    print(`${imm}    ${addr}:\t ${area} (${hex(cur.size_)}) : ${dt}`);
    cur = cur.list_node_.next_;
  }
  print("");
}

const space_tags =
  ['old', 'new_to', 'new_from', 'ro', 'map', 'code', 'lo', 'nlo'];

function get_chunks_space(space_tag, front, chunks) {
    let cur = front;
    while (!cur.isNull) {
        chunks.push({
          'address':cur.address,
          'area_start_':cur.area_start_,
          'area_end_':cur.area_end_,
          'space':space_tag});
        cur = cur.list_node_.next_;
    }
}

function get_chunks() {
  let iso = cast(isolate_address, "v8::internal::Isolate");
  let h = iso.heap_;

  let chunks = [];
  get_chunks_space('old', h.old_space_.memory_chunk_list_.front_, chunks);
  get_chunks_space('new_to',
    h.new_space_.to_space_.memory_chunk_list_.front_, chunks);
  get_chunks_space('new_from',
    h.new_space_.from_space_.memory_chunk_list_.front_, chunks);
  get_chunks_space('ro', h.read_only_space_.memory_chunk_list_.front_, chunks);
  get_chunks_space('map', h.map_space_.memory_chunk_list_.front_, chunks);
  get_chunks_space('code', h.code_space_.memory_chunk_list_.front_, chunks);
  get_chunks_space('lo', h.lo_space_.memory_chunk_list_.front_, chunks);
  get_chunks_space('nlo', h.new_lo_space_.memory_chunk_list_.front_, chunks);

  return chunks;
}

function find_chunk(address) {
  if (!Number.isSafeInteger(int(address))) return undefined;

  let chunks = get_chunks(isolate_address);
  for (let c of chunks) {
    let chunk = cast(c.address, "v8::internal::MemoryChunk");
    if (address >= chunk.area_start_ && address < chunk.area_end_) {
      return c;
    }
  }

  return undefined;
}

function print_memory(space = "all") {
  if (isolate_address == 0) {
    print("Please call !set_iso(isolate_address) first.");
    return;
  }

  let iso = cast(isolate_address, "v8::internal::Isolate");
  let h = iso.heap_;
  print(`Heap at ${h.targetLocation}`);

  let st = space.toLowerCase().split(" ");

  print("Im   address:\t object area start - end (size)");
  if (st.includes("all") || st.includes("old")) {
    print_memory_chunk_list("OldSpace",
      h.old_space_.memory_chunk_list_.front_,
      h.old_space_.allocation_info_.top_);
  }
  if (st.includes("all") || st.includes("new")) {
    // new space doesn't use the chunk list from its base class but from
    // the to/from semi-spaces it points to
    print_memory_chunk_list("NewSpace_To",
      h.new_space_.to_space_.memory_chunk_list_.front_,
      h.new_space_.allocation_info_.top_,
      h.new_space_.to_space_.age_mark_);
    print_memory_chunk_list("NewSpace_From",
      h.new_space_.from_space_.memory_chunk_list_.front_);
  }
  if (st.includes("all") || st.includes("map")) {
    print_memory_chunk_list("MapSpace",
      h.map_space_.memory_chunk_list_.front_,
      h.map_space_.allocation_info_.top_);
  }
  if (st.includes("all") || st.includes("code")) {
    print_memory_chunk_list("CodeSpace",
      h.code_space_.memory_chunk_list_.front_,
      h.code_space_.allocation_info_.top_);
  }
  if (st.includes("all") || st.includes("large") || st.includes("lo")) {
    print_memory_chunk_list("LargeObjectSpace",
      h.lo_space_.memory_chunk_list_.front_);
  }
  if (st.includes("all") || st.includes("newlarge") || st.includes("nlo")) {
    print_memory_chunk_list("NewLargeObjectSpace",
      h.new_lo_space_.memory_chunk_list_.front_);
  }
  if (st.includes("all") || st.includes("readonly") || st.includes("ro")) {
    print_memory_chunk_list("ReadOnlySpace",
      h.read_only_space_.memory_chunk_list_.front_);
  }
}

function print_owning_space(address) {
  if (isolate_address == 0) {
    print("Please call !set_iso(isolate_address) first.");
    return;
  }

  address = decomp(address);
  let c = find_chunk(address);
  if (c) {
      print(`${hex(address)} is in ${c.space} (chunk: ${hex(c.address)})`);
  }
  else {
      print(`Address ${hex(address)} is not in managed heap`);
  }
}

/*-----------------------------------------------------------------------------
    Handles
-----------------------------------------------------------------------------*/
function print_handles_data(print_handles = false) {
  if (isolate_address == 0) {
    print("Please call !set_iso(isolate_address) first.");
    return;
  }

  let iso = cast(isolate_address, "v8::internal::Isolate");
  let hsd = iso.handle_scope_data_;
  let hsimpl = iso.handle_scope_implementer_;

  // depth level
  print(`Nested depth level: ${hsd.level}`);

  // count of handles
  const ptr_size = pointer_size();
  let blocks = hsimpl.blocks_;
  const block_size = 1022; // v8::internal::KB - 2
  const first_block = blocks.data_.address;
  const last_block = (blocks.size_ == 0)
                     ? first_block
                     : first_block + ptr_size * (blocks.size_ - 1);

  const count = (blocks.size_ == 0)
              ? 0
              : (blocks.size_ - 1) * block_size +
                (hsd.next.address - poi(last_block))/ptr_size;
  print(`Currently tracking ${count} local handles`);

  // print the handles
  if (print_handles && count > 0) {
    for (let block = first_block; block < last_block;
         block += block_size * ptr_size) {
      print(`Handles in block at ${hex(block)}`);
      for (let i = 0; i < block_size; i++) {
        const location = poi(block + i * ptr_size);
        print(`  ${hex(location)}->${hex(poi(location))}`);
      }
    }

    let location = poi(last_block);
    print(`Handles in block at ${hex(last_block)}`);
    for (let location = poi(last_block); location < hsd.next.address;
         location += ptr_size) {
      print(`  ${hex(location)}->${hex(poi(location))}`);
    }
  }

  // where will the next handle allocate at?
  const prefix = "Next handle's location will be";
  if (hsd.next.address < hsd.limit.address) {
    print(`${prefix} at ${hex(hsd.next.address)}`);
  }
  else if (hsimpl.spare_) {
    const location = hsimpl.spare_.address;
    print(`${prefix} from the spare block at ${hex(location)}`);
  }
  else {
    print(`${prefix} from a new block to be allocated`);
  }
}

/*-----------------------------------------------------------------------------
    dp
-----------------------------------------------------------------------------*/
function pad_right(addr) {
  let addr_hex = hex(addr);
  return `${addr_hex}${" ".repeat(pointer_size() * 2 + 2 - addr_hex.length)}`;
}

// TODO irinayat: would be nice to identify handles and smi as well
function dp(addr, count = 10) {
  if (isolate_address == 0) {
    print(`To see where objects are located, run !set_iso.`);
  }

  if (!Number.isSafeInteger(int(addr))) {
    print(`${hex(addr)} doesn't look like a valid address`);
    return;
  }

  const ptr_size = tagged_size();
  let aligned_addr = addr - (addr % ptr_size);
  let val = poim(aligned_addr);
  let iter = 0;
  while (val && iter < count) {
    const map = is_map(val);
    const obj = is_likely_object(val) && !map;

    const augm_map = map ? "map" : "";
    const augm_obj = obj ? "obj" : "";
    const augm_other = !map && !obj ? "val" : "";

    let c = find_chunk(decomp(val));
    const augm_space = c ? ` in ${c.space}` : "";
    const augm = `${augm_map}${augm_obj}${augm_other}${augm_space}`;

    const full_ptr = using_ptr_compr ?
        pad_right((map || obj) ? decomp(val) : val) : "";
    print(`${pad_right(aligned_addr)} ${pad_right(val)} ${full_ptr}   ${augm}`);

    aligned_addr += ptr_size;
    val = poim(aligned_addr);
    iter++;
  }
}

/*-----------------------------------------------------------------------------
    Remembered Sets
-----------------------------------------------------------------------------*/
// set ids: 0 = OLD_TO_NEW, 1 = 0 = OLD_TO_OLD
function print_remembered_set(chunk_addr, set_id = 0) {
  if (!chunk_addr) {
    if (isolate_address == 0) {
      print("Please call !set_iso(isolate_address) or provide chunk address.");
      return;
    }

    let iso = cast(isolate_address, "v8::internal::Isolate");
    let h = iso.heap_;
    let chunks = [];
    get_chunks_space('old', h.old_space_.memory_chunk_list_.front_, chunks);
    get_chunks_space('lo', h.lo_space_.memory_chunk_list_.front_, chunks);
    for (let c of chunks) {
      try {
        print_remembered_set(c.address);
      }
      catch (e) {
        print(`failed to process chunk ${hex(c.address)} due to ${e.message}`);
      }
    }
    return;
  }

  print(`Remembered set in chunk ${hex(chunk_addr)}`);
  let chunk = cast(chunk_addr, "v8::internal::MemoryChunk");

  // chunk.slot_set_ is an array of SlotSet's. For standard pages there is 0 or
  // 1 item in the array, but for large pages there will be more.
  const page_size = 256 * 1024;
  const sets_count = Math.floor((chunk.size_ + page_size - 1) / page_size);
  let rs = chunk.slot_set_[set_id];
  if (rs.isNull) {
    print(`  <empty>`);
    return;
  }
  if (rs[0].page_start_ != chunk_addr) {
    print(`page_start_ [${hex(rs.page_start_)}] doesn't match chunk_addr!`);
    return;
  }

  const ptr_size = tagged_size();
  let count = 0;
  for (let s = 0; s < sets_count; s++){
    const buckets_count = rs[s].buckets_.Count();
    for (let b = 0; b < buckets_count; b++) {
      let bucket = rs[s].buckets_[b];
      if (bucket.isNull) continue;
      // there are 32 cells in each bucket, cell's size is 32 bits
      print(`  bucket ${hex(bucket.address.asNumber())}:`);
      const first_cell = bucket.address.asNumber();
      for (let c = 0; c < 32; c++) {
        let cell = host.memory.readMemoryValues(
          first_cell + c * 4, 1, 4 /*size to read*/)[0];
        if (cell == 0) continue;
        let mask = 1;
        for (let bit = 0; bit < 32; bit++){
          if (cell & mask) {
            count++;
            const slot_offset = (b * 32 * 32 + c * 32 + bit) * ptr_size;
            const slot = rs[s].page_start_ + slot_offset;
            print(`    ${hex(slot)} -> ${hex(poim(slot))}`);
          }
          mask = mask << 1;
        }
      }
    }
  }

  if (count == 0) print(`  <empty>`);
  else print(`  ${count} remembered pointers in chunk ${hex(chunk_addr)}`);
}


/*=============================================================================
  Initialize short aliased names for the most common commands
=============================================================================*/
function initializeScript() {
  return [
      new host.functionAlias(help, "help"),
      new host.functionAlias(print_object_from_handle, "jlh"),
      new host.functionAlias(print_object, "job"),
      new host.functionAlias(print_js_stack, "jst"),

      new host.functionAlias(set_isolate_address, "set_iso"),
      new host.functionAlias(module_name, "set_module"),
      new host.functionAlias(print_memory, "mem"),
      new host.functionAlias(print_owning_space, "where"),
      new host.functionAlias(print_handles_data, "handles"),
      new host.functionAlias(print_remembered_set, "rs"),

      new host.functionAlias(print_object_prev, "jo_prev"),
      new host.functionAlias(print_object_next, "jo_next"),
      new host.functionAlias(print_objects_in_range, "jo_in_range"),
      new host.functionAlias(print_objects_tree, "jot"),

      new host.functionAlias(dp, "dp"),

      new host.functionAlias(set_user_js_bp, "jsbp"),
  ]
}
