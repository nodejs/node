// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Normalizes Kat{h,}mandu (chromium:487322)
df = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Katmandu'})
assertEquals('Asia/Katmandu', df.resolvedOptions().timeZone);

df = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Kathmandu'})
assertEquals('Asia/Katmandu', df.resolvedOptions().timeZone);

// Throws for unsupported time zones.
assertThrows(() => Intl.DateTimeFormat(undefined, {timeZone: 'Aurope/Paris'}));
