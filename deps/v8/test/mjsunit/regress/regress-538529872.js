// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --shared-string-table
// Flags: --expose-externalize-string
// Flags: --transition-strings-during-gc-with-stack --gc-global

// The lowercasing builtin unpacks the source, then allocates the result. With
// --gc-global and --transition-strings-during-gc-with-stack, that allocation
// internalizes the source in place and forwards it to a ThinString. Reading it
// as a sequential string afterwards would interpret the ThinString's fields as
// character data, so the builtin must re-check and bail to the runtime.

// Build the content dynamically, so that it is not internalized on creation.
function dyn(s) { return s.slice(0, s.length - 1) + s[s.length - 1]; }

function makeThinnable(content) {
  // Internalize an equal string first, so that the victim below forwards to a
  // distinct object. A string internalized in place is never thinned.
  %ConstructInternalizedString(dyn(content));
  const victim = %ShareObject(dyn(content));
  %ConstructInternalizedString(victim);
  return victim;
}

// Strings up to 24 chars take the CSA lookup-table path, longer ones call
// into C.
for (const length of [16, 64]) {
  const upper = "ABCDEFGHIJKLMNOP".repeat(length / 16);
  const lower = "abcdefghijklmnop".repeat(length / 16);

  const a = makeThinnable(upper);
  %SimulateNewspaceFull();
  assertEquals(lower, a.toLowerCase());

  const b = makeThinnable(upper);
  %SimulateNewspaceFull();
  assertEquals(lower, b.toLocaleLowerCase());

  const c = makeThinnable(upper);
  %SimulateNewspaceFull();
  assertEquals(lower, c.toLocaleLowerCase("en"));
}

// The source can equally be an external string, whose resource internalization
// frees.
{
  const upper = "ABCDEFGHIJKLMNOP";
  %ConstructInternalizedString(dyn(upper));
  const external = createExternalizableString(dyn(upper));
  externalizeString(external);
  const victim = %ShareObject(external);
  %ConstructInternalizedString(victim);

  %SimulateNewspaceFull();
  assertEquals("abcdefghijklmnop", victim.toLowerCase());
}
