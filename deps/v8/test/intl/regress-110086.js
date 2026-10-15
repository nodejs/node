// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --icu-locale=tr

assertEquals('\u0130', 'i'.toLocaleUpperCase());
assertEquals('\u0130', 'i'.toLocaleUpperCase(undefined));
assertEquals('\u0130', 'i'.toLocaleUpperCase([]));

assertEquals('\u0131', 'I'.toLocaleLowerCase());
assertEquals('\u0131', 'I'.toLocaleLowerCase(undefined));
assertEquals('\u0131', 'I'.toLocaleLowerCase([]));
