// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Deconstructing will goto fast path.
const fastProps = { key: "abc", ref: 1234, a: 10, b: 20, c: 30, d: 40, e: 50 };

const { key, ref, ...normalizedFastProps } = fastProps;

// Deconstructing will goto runtime, call
// Runtime:: kCopyDataPropertiesWithExcludedPropertiesOnStack.
// Add a large index to force dictionary elements.
const slowProps = { [2**30] : 10};
assertTrue(%HasDictionaryElements(slowProps));
const { ...normalizedSlowProps } = slowProps;
