// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Environment Variables: LC_ALL=pt_BR
assertEquals(
    'pt-BR',
    (new Intl.RelativeTimeFormat()).resolvedOptions().locale);

assertEquals(
    'pt-BR',
    (new Intl.RelativeTimeFormat([], {style: 'short', numeric: 'auto'}))
    .resolvedOptions().locale);
