// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// d8 harness for the regexp correctness fuzzer.  Reads a JSON array of
// [pattern, flags, subject] triples from the file named on the command line
// and prints one result line per case as "<index>\t<json>".  The result is
// null, an error marker string ("ERR_CTOR" or "ERR_EXEC:<name>"), or
// {r:[captures], idx, li}.  Construction and exec errors are captured rather
// than thrown so a single bad case does not abort the batch; both markers
// share the "ERR_" prefix so the driver can treat either as "no ground
// truth" with a single check.
const cases = JSON.parse(read(arguments[0]));
for (let i = 0; i < cases.length; i++) {
  const [pat, flags, sub] = cases[i];
  let out;
  try {
    const re = new RegExp(pat, flags);
    let m;
    try { m = re.exec(sub); } catch (e) { m = "ERR_EXEC:" + e.name; }
    out = (m === null) ? null
        : (typeof m === "string") ? m
        : {r: Array.from(m, x => x === undefined ? "<u>" : x),
           idx: m.index, li: re.lastIndex};
  } catch (e) { out = "ERR_CTOR"; }
  print(i + "\t" + JSON.stringify(out));
}
print("DONE");
