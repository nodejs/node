// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let cal = (new Intl.DateTimeFormat('en-u-ca-iso8601'))
        .resolvedOptions().calendar;
assertEquals('iso8601', cal);
cal = (new Intl.DateTimeFormat('en-u-ca-ISO8601'))
        .resolvedOptions().calendar;
assertEquals('iso8601', cal);
cal = (new Intl.DateTimeFormat('en', {calendar: 'iso8601'}))
        .resolvedOptions().calendar;
assertEquals('iso8601', cal);
cal = (new Intl.DateTimeFormat('en', {calendar: 'ISO8601'}))
        .resolvedOptions().calendar;
assertEquals('iso8601', cal);
cal = (new Intl.DateTimeFormat('en-u-ca-ISO8601x'))
        .resolvedOptions().calendar;
assertEquals('gregory', cal);
cal = (new Intl.DateTimeFormat('en', {calendar: 'iso8601x'}))
        .resolvedOptions().calendar;
assertEquals('gregory', cal);
