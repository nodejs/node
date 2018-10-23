// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// No locale
var collatorWithOptions = new Intl.Collator(undefined);
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag(%GetDefaultICULocale(), locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator(undefined, {usage: 'sort'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag(%GetDefaultICULocale(), locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator(undefined, {usage: 'search'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertEquals('search', usage);
assertEquals('default', collation);
assertLanguageTag(%GetDefaultICULocale(), locale);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator(locale);
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag(%GetDefaultICULocale(), locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

// With Locale
collatorWithOptions = new Intl.Collator('en-US');
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US', {usage: 'sort'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US', {usage: 'search'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertEquals('search', usage);
assertEquals('default', collation);
assertLanguageTag('en-US', locale);
assertEquals(locale.indexOf('-co-search'), -1);

// With invalid collation value = 'search'
collatorWithOptions = new Intl.Collator('en-US-u-co-search');
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US-u-co-search', {usage: 'sort'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US-u-co-search', {usage: 'search'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('search', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

// With invalid collation value = 'standard'
collatorWithOptions = new Intl.Collator('en-US-u-co-standard');
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US-u-co-standard', {usage: 'sort'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US-u-co-standard', {usage: 'search'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('search', usage);
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);

// With valid collation value = 'emoji'
collatorWithOptions = new Intl.Collator('en-US-u-co-emoji');
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('emoji', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US-u-co-emoji', {usage: 'sort'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('sort', usage);
assertEquals('emoji', collation);
assertEquals(locale.indexOf('-co-search'), -1);

collatorWithOptions = new Intl.Collator('en-US-u-co-emoji', {usage: 'search'});
var { locale, usage, collation } = collatorWithOptions.resolvedOptions();
assertLanguageTag('en-US', locale);
assertEquals('search', usage);
// usage = search overwrites emoji as a collation value.
assertEquals('default', collation);
assertEquals(locale.indexOf('-co-search'), -1);
