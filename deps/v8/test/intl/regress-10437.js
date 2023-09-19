// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertDoesNotThrow(() => (new Intl.NumberFormat(
    'ar', {style: 'unit', unit: 'acre-per-degree'})).format(0));

assertDoesNotThrow(() => (new Intl.NumberFormat(
    'ar', {style: 'unit', unit: 'millimeter-per-mile'})).format(0));

assertDoesNotThrow(() => (new Intl.NumberFormat(
    'ar', {style: 'unit', unit: 'centimeter-per-acre'})).format(1));

assertDoesNotThrow(() => (new Intl.NumberFormat(
    'ar', {style: 'unit', unit: 'minute-per-yard'})).format(1));

assertDoesNotThrow(() => (new Intl.NumberFormat(
    'ar', {style: 'unit', unit: 'foot-per-fluid-ounce'})).format(2));
