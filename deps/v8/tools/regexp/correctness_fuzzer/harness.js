// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs [pattern, flags, subject, lastIndex, tag] cases.  Each result is
// "<index>\t<json>": null, "ERR_CTOR", "ERR_EXEC:<name>", or
// {r:[captures], idx, li}.  Constructor and exec errors do not abort the batch.
//
// harness.py prepends `const cases = [...]`; local and ClusterFuzz drivers use
// the resulting script.
//
// lastIndex is set before exec and read back after: for a sticky or global
// pattern it selects where the attempt starts and is updated by the result, so
// it is part of the observable behaviour being diffed.  It is optional so a
// hand-written repro can omit it.
//
// Each case runs on two separately constructed JSRegExps and both results are
// reported.  The first exec compiles the pattern; what that compile leaves on
// the RegExpData (generated code, quick-check mask, first-character filter)
// reaches the second object through the compilation cache, so the second runs
// warm along paths a single-shot run would never take.
//
// Foozzie uses the tag as a source key for failure clustering.  The
// regexp-fuzzer namespace separates it from JavaScript fuzzer source keys.
for (let i = 0; i < cases.length; i++) {
  const [pat, flags, sub, lastIndex, tag] = cases[i];
  if (tag !== undefined) {
    print("v8-foozzie source: regexp-fuzzer:" + tag);
  }
  let out;
  try {
    const cold = execOnce(pat, flags, sub, lastIndex);
    const warm = execOnce(pat, flags, sub, lastIndex);
    out = {cold: cold, warm: warm};
  } catch (e) { out = "ERR_CTOR"; }
  print(i + "\t" + JSON.stringify(out));
}
print("DONE");

// One construct-and-exec.  Throws only if the constructor does, which is a
// property of the pattern rather than of this attempt, so it fails the whole
// case as ERR_CTOR.
function execOnce(pat, flags, sub, lastIndex) {
  const re = new RegExp(pat, flags);
  if (lastIndex) re.lastIndex = lastIndex;
  let m;
  try { m = re.exec(sub); } catch (e) { return "ERR_EXEC:" + e.name; }
  if (m === null) return null;
  const r = {r: Array.from(m, x => x === undefined ? "<u>" : x),
             idx: m.index, li: re.lastIndex};
  // `d` adds match indices and `(?<name>)` adds the groups object; both are
  // separately constructed result surfaces, so they are diffed too.  Marked
  // explicitly rather than left undefined, since presence is itself a
  // difference worth catching.
  if (m.indices !== undefined) {
    r.di = Array.from(m.indices, x => x === undefined ? "<u>" : x);
    r.dg = m.indices.groups === undefined ? "<u>" : m.indices.groups;
  }
  if (m.groups !== undefined) r.g = m.groups;
  return r;
}
