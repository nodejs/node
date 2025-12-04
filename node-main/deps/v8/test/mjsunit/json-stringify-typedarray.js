// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array,
 Uint32Array, Float32Array, Float64Array]
    .forEach(constructor => {
      const empty = new constructor(0);
      assertEquals('{}', JSON.stringify(empty));

      const tiny = new constructor(2).fill(123);
      assertEquals('{"0":123,"1":123}', JSON.stringify(tiny));

      const huge = new constructor(64).fill(123);
      assertEquals(
          '{"0":123,"1":123,"2":123,"3":123,"4":123,"5":123,"6":123,"7":123,"8":123,"9":123,"10":123,"11":123,"12":123,"13":123,"14":123,"15":123,"16":123,"17":123,"18":123,"19":123,"20":123,"21":123,"22":123,"23":123,"24":123,"25":123,"26":123,"27":123,"28":123,"29":123,"30":123,"31":123,"32":123,"33":123,"34":123,"35":123,"36":123,"37":123,"38":123,"39":123,"40":123,"41":123,"42":123,"43":123,"44":123,"45":123,"46":123,"47":123,"48":123,"49":123,"50":123,"51":123,"52":123,"53":123,"54":123,"55":123,"56":123,"57":123,"58":123,"59":123,"60":123,"61":123,"62":123,"63":123}',
          JSON.stringify(huge));
    });
