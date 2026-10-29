// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --shared-string-table
// Flags: --transition-strings-during-gc-with-stack --gc-global

// StringToArray unpacks the subject, then allocates the result backing store.
// With --gc-global and --transition-strings-during-gc-with-stack, that
// allocation internalizes the subject in place and forwards it to a ThinString,
// leaving the data pointer taken afterwards aimed at unrelated heap data.

// Build the content dynamically, so that it is not internalized on creation.
function dyn(s) { return s.slice(0, s.length - 1) + s[s.length - 1]; }

const content = "ABCDEFGHIJKLMNOP";
// Internalize an equal string first, so that the subject below forwards to a
// distinct object. A string internalized in place is never thinned.
%ConstructInternalizedString(dyn(content));
const subject = %ShareObject(dyn(content));
%ConstructInternalizedString(subject);

// Compute the expected value before simulating a full new space: splitting
// {content} allocates too and would consume the simulated fullness.
const expected = content.split("");

%SimulateNewspaceFull();
assertEquals(expected, subject.split(""));
