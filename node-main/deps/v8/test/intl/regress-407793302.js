// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Locale language should not return undefined but "und"
assertEquals((new Intl.Locale("und")).language, "und");
assertEquals((new Intl.Locale("und-Latn")).language, "und");
assertEquals((new Intl.Locale("und-RS")).language, "und");
