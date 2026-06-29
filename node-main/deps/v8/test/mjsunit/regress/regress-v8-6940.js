// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(/[ŸŶ]/i.test('ÿ'));
assertTrue(/[ŸY]/i.test('ÿ'));

assertTrue(/[YÝŸŶỲ]/i.test('ÿ'));
assertTrue(/[YÝŸŶỲ]/iu.test('ÿ'));
