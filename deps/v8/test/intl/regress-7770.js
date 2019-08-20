// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Environment Variables: TZ=Indian/Kerguelen LANG=uk
assertEquals(
  "Fri Feb 01 2019 00:00:00 GMT+0500 (за часом на Французьких Південних і Антарктичних територіях)",
  new Date(2019, 1,1).toString());
