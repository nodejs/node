// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the fractional operation is calculate in Math Value not double.
// see https://tc39.es/ecma262/#sec-mathematical-operations
assertEquals('0:00:02.712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: 2, milliseconds: 712}));
assertEquals('0:00:17179869194.000000712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: 17179869194, nanoseconds: 712}));
assertEquals('0:00:17179869194.000712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: 17179869194, microseconds: 712}));
assertEquals('0:00:17179869194.712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: 17179869194, milliseconds: 712}));
assertEquals('0:00:17179.869194712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, nanoseconds: 17179869194712}));
assertEquals('0:00:9007199.254740992',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, nanoseconds: 9007199254740992}));

assertEquals('0:00:9007199254740991.000000712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: 9007199254740991, nanoseconds: 712}));

// snormalizedSeconds >= 2^53 (=9007199254740992) seconds is not valid
// https://tc39.es/ecma402/#sec-isvalidduration
assertThrows(() =>
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: 9007199254740992, nanoseconds: 712}));

assertEquals('0:00:00.002712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, milliseconds: 2, microseconds: 712}));
assertEquals('0:00:17179869.194000712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, milliseconds: 17179869194, nanoseconds: 712}));
assertEquals('0:00:17179869.194712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, milliseconds: 17179869194, microseconds: 712}));
assertEquals('0:00:17179.869194712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, microseconds: 17179869194, nanoseconds: 712}));

assertEquals('-0:00:02.712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: -2, milliseconds: -712}));
assertEquals('-0:00:17179869194.000000712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: -17179869194, nanoseconds: -712}));
assertEquals('-0:00:17179869194.000712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: -17179869194, microseconds: -712}));
assertEquals('-0:00:17179869194.712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, seconds: -17179869194, milliseconds: -712}));

assertEquals('-0:00:00.002712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, milliseconds: -2, microseconds: -712}));
assertEquals('-0:00:17179869.194000712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, milliseconds: -17179869194, nanoseconds: -712}));
assertEquals('-0:00:17179869.194712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, milliseconds: -17179869194, microseconds: -712}));
assertEquals('-0:00:17179.869194712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, microseconds: -17179869194, nanoseconds: -712}));

assertEquals('-0:00:17179.869194712',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, nanoseconds: -17179869194712}));
assertEquals('-0:00:9007199.254740992',
    new Intl.DurationFormat('en-US', {style: 'digital'}).format( {hours: 0, minutes: 0, nanoseconds: -9007199254740992}));
