// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// d8 harness for the regexp correctness fuzzer.  Reads a JSON array of
// [pattern, flags, subject, lastIndex] cases from the file named on the command
// line and prints one result line per case as "<index>\t<json>".  The result is
// null, an error marker string ("ERR_CTOR" or "ERR_EXEC:<name>"), or
// {r:[captures], idx, li}.  Construction and exec errors are captured rather
// than thrown so a single bad case does not abort the batch; both markers
// share the "ERR_" prefix so the driver can treat either as "no ground
// truth" with a single check.
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
// Both objects are identical and start from the same lastIndex, so their
// results must agree whatever either configuration computes.  A mismatch is a
// bug in that configuration by itself -- the one class a two-config diff
// cannot see, since it survives being wrong on both sides.
const cases = JSON.parse(read(arguments[0]));
for (let i = 0; i < cases.length; i++) {
  const [pat, flags, sub, lastIndex] = cases[i];
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
