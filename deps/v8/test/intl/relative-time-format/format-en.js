// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following test are not part of the comformance. Just some output in
// English to verify the format does return something reasonable for English.
// It may be changed when we update the CLDR data.
// NOTE: These are UNSPECIFIED behavior in
// http://tc39.github.io/proposal-intl-relative-time/

let longAuto = new Intl.RelativeTimeFormat(
    "en", {style: "long", localeMatcher: 'lookup', numeric: 'auto'});

assertEquals('3 seconds ago', longAuto.format(-3, 'second'));
assertEquals('2 seconds ago', longAuto.format(-2, 'second'));
assertEquals('1 second ago', longAuto.format(-1, 'second'));
assertEquals('now', longAuto.format(0, 'second'));
assertEquals('now', longAuto.format(-0, 'second'));
assertEquals('in 1 second', longAuto.format(1, 'second'));
assertEquals('in 2 seconds', longAuto.format(2, 'second'));
assertEquals('in 345 seconds', longAuto.format(345, 'second'));

assertEquals('3 minutes ago', longAuto.format(-3, 'minute'));
assertEquals('2 minutes ago', longAuto.format(-2, 'minute'));
assertEquals('1 minute ago', longAuto.format(-1, 'minute'));
assertEquals('in 0 minutes', longAuto.format(0, 'minute'));
assertEquals('0 minutes ago', longAuto.format(-0, 'minute'));
assertEquals('in 1 minute', longAuto.format(1, 'minute'));
assertEquals('in 2 minutes', longAuto.format(2, 'minute'));
assertEquals('in 345 minutes', longAuto.format(345, 'minute'));

assertEquals('3 hours ago', longAuto.format(-3, 'hour'));
assertEquals('2 hours ago', longAuto.format(-2, 'hour'));
assertEquals('1 hour ago', longAuto.format(-1, 'hour'));
assertEquals('in 0 hours', longAuto.format(0, 'hour'));
assertEquals('0 hours ago', longAuto.format(-0, 'hour'));
assertEquals('in 1 hour', longAuto.format(1, 'hour'));
assertEquals('in 2 hours', longAuto.format(2, 'hour'));
assertEquals('in 345 hours', longAuto.format(345, 'hour'));

assertEquals('3 days ago', longAuto.format(-3, 'day'));
assertEquals('2 days ago', longAuto.format(-2, 'day'));
assertEquals('yesterday', longAuto.format(-1, 'day'));
assertEquals('today', longAuto.format(0, 'day'));
assertEquals('today', longAuto.format(-0, 'day'));
assertEquals('tomorrow', longAuto.format(1, 'day'));
assertEquals('in 2 days', longAuto.format(2, 'day'));
assertEquals('in 345 days', longAuto.format(345, 'day'));

assertEquals('3 weeks ago', longAuto.format(-3, 'week'));
assertEquals('2 weeks ago', longAuto.format(-2, 'week'));
assertEquals('last week', longAuto.format(-1, 'week'));
assertEquals('this week', longAuto.format(0, 'week'));
assertEquals('this week', longAuto.format(-0, 'week'));
assertEquals('next week', longAuto.format(1, 'week'));
assertEquals('in 2 weeks', longAuto.format(2, 'week'));
assertEquals('in 345 weeks', longAuto.format(345, 'week'));

assertEquals('3 months ago', longAuto.format(-3, 'month'));
assertEquals('2 months ago', longAuto.format(-2, 'month'));
assertEquals('last month', longAuto.format(-1, 'month'));
assertEquals('this month', longAuto.format(0, 'month'));
assertEquals('this month', longAuto.format(-0, 'month'));
assertEquals('next month', longAuto.format(1, 'month'));
assertEquals('in 2 months', longAuto.format(2, 'month'));
assertEquals('in 345 months', longAuto.format(345, 'month'));

assertEquals('3 quarters ago', longAuto.format(-3, 'quarter'));
assertEquals('2 quarters ago', longAuto.format(-2, 'quarter'));
assertEquals('last quarter', longAuto.format(-1, 'quarter'));
assertEquals('this quarter', longAuto.format(0, 'quarter'));
assertEquals('this quarter', longAuto.format(-0, 'quarter'));
assertEquals('next quarter', longAuto.format(1, 'quarter'));
assertEquals('in 2 quarters', longAuto.format(2, 'quarter'));
assertEquals('in 345 quarters', longAuto.format(345, 'quarter'));

assertEquals('3 years ago', longAuto.format(-3, 'year'));
assertEquals('2 years ago', longAuto.format(-2, 'year'));
assertEquals('last year', longAuto.format(-1, 'year'));
assertEquals('this year', longAuto.format(0, 'year'));
assertEquals('this year', longAuto.format(-0, 'year'));
assertEquals('next year', longAuto.format(1, 'year'));
assertEquals('in 2 years', longAuto.format(2, 'year'));
assertEquals('in 345 years', longAuto.format(345, 'year'));

let shortAuto = new Intl.RelativeTimeFormat(
    "en", {style: "short", localeMatcher: 'lookup', numeric: 'auto'});

assertEquals('3 sec. ago', shortAuto.format(-3, 'second'));
assertEquals('2 sec. ago', shortAuto.format(-2, 'second'));
assertEquals('1 sec. ago', shortAuto.format(-1, 'second'));
assertEquals('now', shortAuto.format(0, 'second'));
assertEquals('now', shortAuto.format(-0, 'second'));
assertEquals('in 1 sec.', shortAuto.format(1, 'second'));
assertEquals('in 2 sec.', shortAuto.format(2, 'second'));
assertEquals('in 345 sec.', shortAuto.format(345, 'second'));

assertEquals('3 min. ago', shortAuto.format(-3, 'minute'));
assertEquals('2 min. ago', shortAuto.format(-2, 'minute'));
assertEquals('1 min. ago', shortAuto.format(-1, 'minute'));
assertEquals('in 0 min.', shortAuto.format(0, 'minute'));
assertEquals('0 min. ago', shortAuto.format(-0, 'minute'));
assertEquals('in 1 min.', shortAuto.format(1, 'minute'));
assertEquals('in 2 min.', shortAuto.format(2, 'minute'));
assertEquals('in 345 min.', shortAuto.format(345, 'minute'));

assertEquals('3 hr. ago', shortAuto.format(-3, 'hour'));
assertEquals('2 hr. ago', shortAuto.format(-2, 'hour'));
assertEquals('1 hr. ago', shortAuto.format(-1, 'hour'));
assertEquals('in 0 hr.', shortAuto.format(0, 'hour'));
assertEquals('0 hr. ago', shortAuto.format(-0, 'hour'));
assertEquals('in 1 hr.', shortAuto.format(1, 'hour'));
assertEquals('in 2 hr.', shortAuto.format(2, 'hour'));
assertEquals('in 345 hr.', shortAuto.format(345, 'hour'));

assertEquals('3 days ago', shortAuto.format(-3, 'day'));
assertEquals('2 days ago', shortAuto.format(-2, 'day'));
assertEquals('yesterday', shortAuto.format(-1, 'day'));
assertEquals('today', shortAuto.format(0, 'day'));
assertEquals('today', shortAuto.format(-0, 'day'));
assertEquals('tomorrow', shortAuto.format(1, 'day'));
assertEquals('in 2 days', shortAuto.format(2, 'day'));
assertEquals('in 345 days', shortAuto.format(345, 'day'));

assertEquals('3 wk. ago', shortAuto.format(-3, 'week'));
assertEquals('2 wk. ago', shortAuto.format(-2, 'week'));
assertEquals('last wk.', shortAuto.format(-1, 'week'));
assertEquals('this wk.', shortAuto.format(0, 'week'));
assertEquals('this wk.', shortAuto.format(-0, 'week'));
assertEquals('next wk.', shortAuto.format(1, 'week'));
assertEquals('in 2 wk.', shortAuto.format(2, 'week'));
assertEquals('in 345 wk.', shortAuto.format(345, 'week'));

assertEquals('3 mo. ago', shortAuto.format(-3, 'month'));
assertEquals('2 mo. ago', shortAuto.format(-2, 'month'));
assertEquals('last mo.', shortAuto.format(-1, 'month'));
assertEquals('this mo.', shortAuto.format(0, 'month'));
assertEquals('this mo.', shortAuto.format(-0, 'month'));
assertEquals('next mo.', shortAuto.format(1, 'month'));
assertEquals('in 2 mo.', shortAuto.format(2, 'month'));
assertEquals('in 345 mo.', shortAuto.format(345, 'month'));

assertEquals('3 qtrs. ago', shortAuto.format(-3, 'quarter'));
assertEquals('2 qtrs. ago', shortAuto.format(-2, 'quarter'));
assertEquals('last qtr.', shortAuto.format(-1, 'quarter'));
assertEquals('this qtr.', shortAuto.format(0, 'quarter'));
assertEquals('this qtr.', shortAuto.format(-0, 'quarter'));
assertEquals('next qtr.', shortAuto.format(1, 'quarter'));
assertEquals('in 2 qtrs.', shortAuto.format(2, 'quarter'));
assertEquals('in 345 qtrs.', shortAuto.format(345, 'quarter'));

assertEquals('3 yr. ago', shortAuto.format(-3, 'year'));
assertEquals('2 yr. ago', shortAuto.format(-2, 'year'));
assertEquals('last yr.', shortAuto.format(-1, 'year'));
assertEquals('this yr.', shortAuto.format(0, 'year'));
assertEquals('this yr.', shortAuto.format(-0, 'year'));
assertEquals('next yr.', shortAuto.format(1, 'year'));
assertEquals('in 2 yr.', shortAuto.format(2, 'year'));
assertEquals('in 345 yr.', shortAuto.format(345, 'year'));

// Somehow in the 'en' locale, there are no valeu for -narrow
let narrowAuto = new Intl.RelativeTimeFormat(
    "en", {style: "narrow", localeMatcher: 'lookup', numeric: 'auto'});

assertEquals('3 sec. ago', narrowAuto.format(-3, 'second'));
assertEquals('2 sec. ago', narrowAuto.format(-2, 'second'));
assertEquals('1 sec. ago', narrowAuto.format(-1, 'second'));
assertEquals('now', narrowAuto.format(0, 'second'));
assertEquals('now', narrowAuto.format(-0, 'second'));
assertEquals('in 1 sec.', narrowAuto.format(1, 'second'));
assertEquals('in 2 sec.', narrowAuto.format(2, 'second'));
assertEquals('in 345 sec.', narrowAuto.format(345, 'second'));

assertEquals('3 min. ago', narrowAuto.format(-3, 'minute'));
assertEquals('2 min. ago', narrowAuto.format(-2, 'minute'));
assertEquals('1 min. ago', narrowAuto.format(-1, 'minute'));
assertEquals('in 0 min.', narrowAuto.format(0, 'minute'));
assertEquals('0 min. ago', narrowAuto.format(-0, 'minute'));
assertEquals('in 1 min.', narrowAuto.format(1, 'minute'));
assertEquals('in 2 min.', narrowAuto.format(2, 'minute'));
assertEquals('in 345 min.', narrowAuto.format(345, 'minute'));

assertEquals('3 hr. ago', narrowAuto.format(-3, 'hour'));
assertEquals('2 hr. ago', narrowAuto.format(-2, 'hour'));
assertEquals('1 hr. ago', narrowAuto.format(-1, 'hour'));
assertEquals('in 0 hr.', narrowAuto.format(0, 'hour'));
assertEquals('0 hr. ago', narrowAuto.format(-0, 'hour'));
assertEquals('in 1 hr.', narrowAuto.format(1, 'hour'));
assertEquals('in 2 hr.', narrowAuto.format(2, 'hour'));
assertEquals('in 345 hr.', narrowAuto.format(345, 'hour'));

assertEquals('3 days ago', narrowAuto.format(-3, 'day'));
assertEquals('2 days ago', narrowAuto.format(-2, 'day'));
assertEquals('yesterday', narrowAuto.format(-1, 'day'));
assertEquals('today', narrowAuto.format(0, 'day'));
assertEquals('today', narrowAuto.format(-0, 'day'));
assertEquals('tomorrow', narrowAuto.format(1, 'day'));
assertEquals('in 2 days', narrowAuto.format(2, 'day'));
assertEquals('in 345 days', narrowAuto.format(345, 'day'));

assertEquals('3 wk. ago', narrowAuto.format(-3, 'week'));
assertEquals('2 wk. ago', narrowAuto.format(-2, 'week'));
assertEquals('last wk.', narrowAuto.format(-1, 'week'));
assertEquals('this wk.', narrowAuto.format(0, 'week'));
assertEquals('this wk.', narrowAuto.format(-0, 'week'));
assertEquals('next wk.', narrowAuto.format(1, 'week'));
assertEquals('in 2 wk.', narrowAuto.format(2, 'week'));
assertEquals('in 345 wk.', narrowAuto.format(345, 'week'));

assertEquals('3 mo. ago', narrowAuto.format(-3, 'month'));
assertEquals('2 mo. ago', narrowAuto.format(-2, 'month'));
assertEquals('last mo.', narrowAuto.format(-1, 'month'));
assertEquals('this mo.', narrowAuto.format(0, 'month'));
assertEquals('this mo.', narrowAuto.format(-0, 'month'));
assertEquals('next mo.', narrowAuto.format(1, 'month'));
assertEquals('in 2 mo.', narrowAuto.format(2, 'month'));
assertEquals('in 345 mo.', narrowAuto.format(345, 'month'));

assertEquals('3 qtrs. ago', narrowAuto.format(-3, 'quarter'));
assertEquals('2 qtrs. ago', narrowAuto.format(-2, 'quarter'));
assertEquals('last qtr.', narrowAuto.format(-1, 'quarter'));
assertEquals('this qtr.', narrowAuto.format(0, 'quarter'));
assertEquals('this qtr.', narrowAuto.format(-0, 'quarter'));
assertEquals('next qtr.', narrowAuto.format(1, 'quarter'));
assertEquals('in 2 qtrs.', narrowAuto.format(2, 'quarter'));
assertEquals('in 345 qtrs.', narrowAuto.format(345, 'quarter'));

assertEquals('3 yr. ago', narrowAuto.format(-3, 'year'));
assertEquals('2 yr. ago', narrowAuto.format(-2, 'year'));
assertEquals('last yr.', narrowAuto.format(-1, 'year'));
assertEquals('this yr.', narrowAuto.format(0, 'year'));
assertEquals('this yr.', narrowAuto.format(-0, 'year'));
assertEquals('next yr.', narrowAuto.format(1, 'year'));
assertEquals('in 2 yr.', narrowAuto.format(2, 'year'));
assertEquals('in 345 yr.', narrowAuto.format(345, 'year'));

let longAlways = new Intl.RelativeTimeFormat(
    "en", {style: "long", localeMatcher: 'lookup', numeric: 'always'});

assertEquals('3 seconds ago', longAlways.format(-3, 'second'));
assertEquals('2 seconds ago', longAlways.format(-2, 'second'));
assertEquals('1 second ago', longAlways.format(-1, 'second'));
assertEquals('in 0 seconds', longAlways.format(0, 'second'));
assertEquals('0 seconds ago', longAlways.format(-0, 'second'));
assertEquals('in 1 second', longAlways.format(1, 'second'));
assertEquals('in 2 seconds', longAlways.format(2, 'second'));
assertEquals('in 345 seconds', longAlways.format(345, 'second'));

assertEquals('3 minutes ago', longAlways.format(-3, 'minute'));
assertEquals('2 minutes ago', longAlways.format(-2, 'minute'));
assertEquals('1 minute ago', longAlways.format(-1, 'minute'));
assertEquals('in 0 minutes', longAlways.format(0, 'minute'));
assertEquals('0 minutes ago', longAlways.format(-0, 'minute'));
assertEquals('in 1 minute', longAlways.format(1, 'minute'));
assertEquals('in 2 minutes', longAlways.format(2, 'minute'));
assertEquals('in 345 minutes', longAlways.format(345, 'minute'));

assertEquals('3 hours ago', longAlways.format(-3, 'hour'));
assertEquals('2 hours ago', longAlways.format(-2, 'hour'));
assertEquals('1 hour ago', longAlways.format(-1, 'hour'));
assertEquals('in 0 hours', longAlways.format(0, 'hour'));
assertEquals('0 hours ago', longAlways.format(-0, 'hour'));
assertEquals('in 1 hour', longAlways.format(1, 'hour'));
assertEquals('in 2 hours', longAlways.format(2, 'hour'));
assertEquals('in 345 hours', longAlways.format(345, 'hour'));

assertEquals('3 days ago', longAlways.format(-3, 'day'));
assertEquals('2 days ago', longAlways.format(-2, 'day'));
assertEquals('1 day ago', longAlways.format(-1, 'day'));
assertEquals('in 0 days', longAlways.format(0, 'day'));
assertEquals('0 days ago', longAlways.format(-0, 'day'));
assertEquals('in 1 day', longAlways.format(1, 'day'));
assertEquals('in 2 days', longAlways.format(2, 'day'));
assertEquals('in 345 days', longAlways.format(345, 'day'));

assertEquals('3 weeks ago', longAlways.format(-3, 'week'));
assertEquals('2 weeks ago', longAlways.format(-2, 'week'));
assertEquals('1 week ago', longAlways.format(-1, 'week'));
assertEquals('in 0 weeks', longAlways.format(0, 'week'));
assertEquals('0 weeks ago', longAlways.format(-0, 'week'));
assertEquals('in 1 week', longAlways.format(1, 'week'));
assertEquals('in 2 weeks', longAlways.format(2, 'week'));
assertEquals('in 345 weeks', longAlways.format(345, 'week'));

assertEquals('3 months ago', longAlways.format(-3, 'month'));
assertEquals('2 months ago', longAlways.format(-2, 'month'));
assertEquals('1 month ago', longAlways.format(-1, 'month'));
assertEquals('in 0 months', longAlways.format(0, 'month'));
assertEquals('0 months ago', longAlways.format(-0, 'month'));
assertEquals('in 1 month', longAlways.format(1, 'month'));
assertEquals('in 2 months', longAlways.format(2, 'month'));
assertEquals('in 345 months', longAlways.format(345, 'month'));

assertEquals('3 quarters ago', longAlways.format(-3, 'quarter'));
assertEquals('2 quarters ago', longAlways.format(-2, 'quarter'));
assertEquals('1 quarter ago', longAlways.format(-1, 'quarter'));
assertEquals('in 0 quarters', longAlways.format(0, 'quarter'));
assertEquals('0 quarters ago', longAlways.format(-0, 'quarter'));
assertEquals('in 1 quarter', longAlways.format(1, 'quarter'));
assertEquals('in 2 quarters', longAlways.format(2, 'quarter'));
assertEquals('in 345 quarters', longAlways.format(345, 'quarter'));

assertEquals('3 years ago', longAlways.format(-3, 'year'));
assertEquals('2 years ago', longAlways.format(-2, 'year'));
assertEquals('1 year ago', longAlways.format(-1, 'year'));
assertEquals('in 0 years', longAlways.format(0, 'year'));
assertEquals('0 years ago', longAlways.format(-0, 'year'));
assertEquals('in 1 year', longAlways.format(1, 'year'));
assertEquals('in 2 years', longAlways.format(2, 'year'));
assertEquals('in 345 years', longAlways.format(345, 'year'));

let shortAlways = new Intl.RelativeTimeFormat(
    "en", {style: "short", localeMatcher: 'lookup', numeric: 'always'});

assertEquals('3 sec. ago', shortAlways.format(-3, 'second'));
assertEquals('2 sec. ago', shortAlways.format(-2, 'second'));
assertEquals('1 sec. ago', shortAlways.format(-1, 'second'));
assertEquals('in 0 sec.', shortAlways.format(0, 'second'));
assertEquals('0 sec. ago', shortAlways.format(-0, 'second'));
assertEquals('in 1 sec.', shortAlways.format(1, 'second'));
assertEquals('in 2 sec.', shortAlways.format(2, 'second'));
assertEquals('in 345 sec.', shortAlways.format(345, 'second'));

assertEquals('3 min. ago', shortAlways.format(-3, 'minute'));
assertEquals('2 min. ago', shortAlways.format(-2, 'minute'));
assertEquals('1 min. ago', shortAlways.format(-1, 'minute'));
assertEquals('in 0 min.', shortAlways.format(0, 'minute'));
assertEquals('0 min. ago', shortAlways.format(-0, 'minute'));
assertEquals('in 1 min.', shortAlways.format(1, 'minute'));
assertEquals('in 2 min.', shortAlways.format(2, 'minute'));
assertEquals('in 345 min.', shortAlways.format(345, 'minute'));

assertEquals('3 hr. ago', shortAlways.format(-3, 'hour'));
assertEquals('2 hr. ago', shortAlways.format(-2, 'hour'));
assertEquals('1 hr. ago', shortAlways.format(-1, 'hour'));
assertEquals('in 0 hr.', shortAlways.format(0, 'hour'));
assertEquals('0 hr. ago', shortAlways.format(-0, 'hour'));
assertEquals('in 1 hr.', shortAlways.format(1, 'hour'));
assertEquals('in 2 hr.', shortAlways.format(2, 'hour'));
assertEquals('in 345 hr.', shortAlways.format(345, 'hour'));

assertEquals('3 days ago', shortAlways.format(-3, 'day'));
assertEquals('2 days ago', shortAlways.format(-2, 'day'));
assertEquals('1 day ago', shortAlways.format(-1, 'day'));
assertEquals('in 0 days', shortAlways.format(0, 'day'));
assertEquals('0 days ago', shortAlways.format(-0, 'day'));
assertEquals('in 1 day', shortAlways.format(1, 'day'));
assertEquals('in 2 days', shortAlways.format(2, 'day'));
assertEquals('in 345 days', shortAlways.format(345, 'day'));

assertEquals('3 wk. ago', shortAlways.format(-3, 'week'));
assertEquals('2 wk. ago', shortAlways.format(-2, 'week'));
assertEquals('1 wk. ago', shortAlways.format(-1, 'week'));
assertEquals('in 0 wk.', shortAlways.format(0, 'week'));
assertEquals('0 wk. ago', shortAlways.format(-0, 'week'));
assertEquals('in 1 wk.', shortAlways.format(1, 'week'));
assertEquals('in 2 wk.', shortAlways.format(2, 'week'));
assertEquals('in 345 wk.', shortAlways.format(345, 'week'));

assertEquals('3 mo. ago', shortAlways.format(-3, 'month'));
assertEquals('2 mo. ago', shortAlways.format(-2, 'month'));
assertEquals('1 mo. ago', shortAlways.format(-1, 'month'));
assertEquals('in 0 mo.', shortAlways.format(0, 'month'));
assertEquals('0 mo. ago', shortAlways.format(-0, 'month'));
assertEquals('in 1 mo.', shortAlways.format(1, 'month'));
assertEquals('in 2 mo.', shortAlways.format(2, 'month'));
assertEquals('in 345 mo.', shortAlways.format(345, 'month'));

assertEquals('3 qtrs. ago', shortAlways.format(-3, 'quarter'));
assertEquals('2 qtrs. ago', shortAlways.format(-2, 'quarter'));
assertEquals('1 qtr. ago', shortAlways.format(-1, 'quarter'));
assertEquals('in 0 qtrs.', shortAlways.format(0, 'quarter'));
assertEquals('0 qtrs. ago', shortAlways.format(-0, 'quarter'));
assertEquals('in 1 qtr.', shortAlways.format(1, 'quarter'));
assertEquals('in 2 qtrs.', shortAlways.format(2, 'quarter'));
assertEquals('in 345 qtrs.', shortAlways.format(345, 'quarter'));

assertEquals('3 yr. ago', shortAlways.format(-3, 'year'));
assertEquals('2 yr. ago', shortAlways.format(-2, 'year'));
assertEquals('1 yr. ago', shortAlways.format(-1, 'year'));
assertEquals('in 0 yr.', shortAlways.format(0, 'year'));
assertEquals('0 yr. ago', shortAlways.format(-0, 'year'));
assertEquals('in 1 yr.', shortAlways.format(1, 'year'));
assertEquals('in 2 yr.', shortAlways.format(2, 'year'));
assertEquals('in 345 yr.', shortAlways.format(345, 'year'));

// Somehow in the 'en' locale, there are no valeu for -narrow
let narrowAlways = new Intl.RelativeTimeFormat(
    "en", {style: "narrow", localeMatcher: 'lookup', numeric: 'always'});

assertEquals('3 sec. ago', narrowAlways.format(-3, 'second'));
assertEquals('2 sec. ago', narrowAlways.format(-2, 'second'));
assertEquals('1 sec. ago', narrowAlways.format(-1, 'second'));
assertEquals('in 0 sec.', narrowAlways.format(0, 'second'));
assertEquals('0 sec. ago', narrowAlways.format(-0, 'second'));
assertEquals('in 1 sec.', narrowAlways.format(1, 'second'));
assertEquals('in 2 sec.', narrowAlways.format(2, 'second'));
assertEquals('in 345 sec.', narrowAlways.format(345, 'second'));

assertEquals('3 min. ago', narrowAlways.format(-3, 'minute'));
assertEquals('2 min. ago', narrowAlways.format(-2, 'minute'));
assertEquals('1 min. ago', narrowAlways.format(-1, 'minute'));
assertEquals('in 0 min.', narrowAlways.format(0, 'minute'));
assertEquals('0 min. ago', narrowAlways.format(-0, 'minute'));
assertEquals('in 1 min.', narrowAlways.format(1, 'minute'));
assertEquals('in 2 min.', narrowAlways.format(2, 'minute'));
assertEquals('in 345 min.', narrowAlways.format(345, 'minute'));

assertEquals('3 hr. ago', narrowAlways.format(-3, 'hour'));
assertEquals('2 hr. ago', narrowAlways.format(-2, 'hour'));
assertEquals('1 hr. ago', narrowAlways.format(-1, 'hour'));
assertEquals('in 0 hr.', narrowAlways.format(0, 'hour'));
assertEquals('0 hr. ago', narrowAlways.format(-0, 'hour'));
assertEquals('in 1 hr.', narrowAlways.format(1, 'hour'));
assertEquals('in 2 hr.', narrowAlways.format(2, 'hour'));
assertEquals('in 345 hr.', narrowAlways.format(345, 'hour'));

assertEquals('3 days ago', narrowAlways.format(-3, 'day'));
assertEquals('2 days ago', narrowAlways.format(-2, 'day'));
assertEquals('1 day ago', narrowAlways.format(-1, 'day'));
assertEquals('in 0 days', narrowAlways.format(0, 'day'));
assertEquals('0 days ago', narrowAlways.format(-0, 'day'));
assertEquals('in 1 day', narrowAlways.format(1, 'day'));
assertEquals('in 2 days', narrowAlways.format(2, 'day'));
assertEquals('in 345 days', narrowAlways.format(345, 'day'));

assertEquals('3 wk. ago', narrowAlways.format(-3, 'week'));
assertEquals('2 wk. ago', narrowAlways.format(-2, 'week'));
assertEquals('1 wk. ago', narrowAlways.format(-1, 'week'));
assertEquals('in 0 wk.', narrowAlways.format(0, 'week'));
assertEquals('0 wk. ago', narrowAlways.format(-0, 'week'));
assertEquals('in 1 wk.', narrowAlways.format(1, 'week'));
assertEquals('in 2 wk.', narrowAlways.format(2, 'week'));
assertEquals('in 345 wk.', narrowAlways.format(345, 'week'));

assertEquals('3 mo. ago', narrowAlways.format(-3, 'month'));
assertEquals('2 mo. ago', narrowAlways.format(-2, 'month'));
assertEquals('1 mo. ago', narrowAlways.format(-1, 'month'));
assertEquals('in 0 mo.', narrowAlways.format(0, 'month'));
assertEquals('0 mo. ago', narrowAlways.format(-0, 'month'));
assertEquals('in 1 mo.', narrowAlways.format(1, 'month'));
assertEquals('in 2 mo.', narrowAlways.format(2, 'month'));
assertEquals('in 345 mo.', narrowAlways.format(345, 'month'));

assertEquals('3 qtrs. ago', narrowAlways.format(-3, 'quarter'));
assertEquals('2 qtrs. ago', narrowAlways.format(-2, 'quarter'));
assertEquals('1 qtr. ago', narrowAlways.format(-1, 'quarter'));
assertEquals('in 0 qtrs.', narrowAlways.format(0, 'quarter'));
assertEquals('0 qtrs. ago', narrowAlways.format(-0, 'quarter'));
assertEquals('in 1 qtr.', narrowAlways.format(1, 'quarter'));
assertEquals('in 2 qtrs.', narrowAlways.format(2, 'quarter'));
assertEquals('in 345 qtrs.', narrowAlways.format(345, 'quarter'));

assertEquals('3 yr. ago', narrowAlways.format(-3, 'year'));
assertEquals('2 yr. ago', narrowAlways.format(-2, 'year'));
assertEquals('1 yr. ago', narrowAlways.format(-1, 'year'));
assertEquals('in 0 yr.', narrowAlways.format(0, 'year'));
assertEquals('0 yr. ago', narrowAlways.format(-0, 'year'));
assertEquals('in 1 yr.', narrowAlways.format(1, 'year'));
assertEquals('in 2 yr.', narrowAlways.format(2, 'year'));
assertEquals('in 345 yr.', narrowAlways.format(345, 'year'));

var styleNumericCombinations = [
    longAuto, shortAuto, narrowAuto, longAlways,
    shortAlways, narrowAlways ];
var validUnits = [
    'second', 'minute', 'hour', 'day', 'week', 'month', 'quarter', 'year'];

// Test these all throw RangeError
for (var i = 0; i < styleNumericCombinations.length; i++) {
  for (var j = 0; j < validUnits.length; j++) {
    assertThrows(() => styleNumericCombinations[i].format(NaN, validUnits[j]),
        RangeError);
    assertThrows(() => styleNumericCombinations[i].format(NaN, validUnits[j] + 's'),
        RangeError);
    assertThrows(() => styleNumericCombinations[i].format(NaN, validUnits[j]),
        RangeError);
    assertThrows(() => styleNumericCombinations[i].format(NaN, validUnits[j] + 's'),
        RangeError);
  }
}
