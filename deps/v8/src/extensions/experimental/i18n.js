// Copyright 2006-2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// TODO(cira): Rename v8Locale into LocaleInfo once we have stable API.
/**
 * LocaleInfo class is an aggregate class of all i18n API calls.
 * @param {Object} settings - localeID and regionID to create LocaleInfo from.
 *   {Array.<string>|string} settings.localeID -
 *     Unicode identifier of the locale.
 *     See http://unicode.org/reports/tr35/#BCP_47_Conformance
 *   {string} settings.regionID - ISO3166 region ID with addition of
 *     invalid, undefined and reserved region codes.
 * @constructor
 */
v8Locale = function(settings) {
  native function NativeJSLocale();

  // Assume user wanted to do v8Locale("sr");
  if (typeof(settings) === "string") {
    settings = {'localeID': settings};
  }

  var properties = NativeJSLocale(
      v8Locale.__createSettingsOrDefault(settings, {'localeID': 'root'}));

  // Keep the resolved ICU locale ID around to avoid resolving localeID to
  // ICU locale ID every time BreakIterator, Collator and so forth are called.
  this.__icuLocaleID = properties.icuLocaleID;
  this.options = {'localeID': properties.localeID,
                  'regionID': properties.regionID};
};

/**
 * Clones existing locale with possible overrides for some of the options.
 * @param {!Object} settings - overrides for current locale settings.
 * @returns {Object} - new LocaleInfo object.
 */
v8Locale.prototype.derive = function(settings) {
  return new v8Locale(
      v8Locale.__createSettingsOrDefault(settings, this.options));
};

/**
 * v8BreakIterator class implements locale aware segmenatation.
 * It is not part of EcmaScript proposal.
 * @param {Object} locale - locale object to pass to break
 *   iterator implementation.
 * @param {string} type - type of segmenatation:
 *   - character
 *   - word
 *   - sentence
 *   - line
 * @private
 * @constructor
 */
v8Locale.v8BreakIterator = function(locale, type) {
  native function NativeJSBreakIterator();

  locale = v8Locale.__createLocaleOrDefault(locale);
  // BCP47 ID would work in this case, but we use ICU locale for consistency.
  var iterator = NativeJSBreakIterator(locale.__icuLocaleID, type);
  iterator.type = type;
  return iterator;
};

/**
 * Type of the break we encountered during previous iteration.
 * @type{Enum}
 */
v8Locale.v8BreakIterator.BreakType = {
  'unknown': -1,
  'none': 0,
  'number': 100,
  'word': 200,
  'kana': 300,
  'ideo': 400
};

/**
 * Creates new v8BreakIterator based on current locale.
 * @param {string} - type of segmentation. See constructor.
 * @returns {Object} - new v8BreakIterator object.
 */
v8Locale.prototype.v8CreateBreakIterator = function(type) {
  return new v8Locale.v8BreakIterator(this, type);
};

// TODO(jungshik): Set |collator.options| to actually recognized / resolved
// values.
/**
 * Collator class implements locale-aware sort.
 * @param {Object} locale - locale object to pass to collator implementation.
 * @param {Object} settings - collation flags:
 *   - ignoreCase
 *   - ignoreAccents
 *   - numeric
 * @private
 * @constructor
 */
v8Locale.Collator = function(locale, settings) {
  native function NativeJSCollator();

  locale = v8Locale.__createLocaleOrDefault(locale);
  var collator = NativeJSCollator(
      locale.__icuLocaleID, v8Locale.__createSettingsOrDefault(settings, {}));
  return collator;
};

/**
 * Creates new Collator based on current locale.
 * @param {Object} - collation flags. See constructor.
 * @returns {Object} - new Collator object.
 */
v8Locale.prototype.createCollator = function(settings) {
  return new v8Locale.Collator(this, settings);
};

/**
 * DateTimeFormat class implements locale-aware date and time formatting.
 * Constructor is not part of public API.
 * @param {Object} locale - locale object to pass to formatter.
 * @param {Object} settings - formatting flags:
 *   - skeleton
 *   - dateStyle
 *   - timeStyle
 * @private
 * @constructor
 */
v8Locale.__DateTimeFormat = function(locale, settings) {
  native function NativeJSDateTimeFormat();

  settings = v8Locale.__createSettingsOrDefault(settings, {});

  var cleanSettings = {};
  if (settings.hasOwnProperty('skeleton')) {
    cleanSettings['skeleton'] = settings['skeleton'];
  } else {
    cleanSettings = {};
    if (settings.hasOwnProperty('dateStyle')) {
      var ds = settings['dateStyle'];
      if (!/^(short|medium|long|full)$/.test(ds)) ds = 'short';
      cleanSettings['dateStyle'] = ds;
    } else if (settings.hasOwnProperty('dateType')) {
      // Obsolete. New spec requires dateStyle, but we'll keep this around
      // for current users.
      // TODO(cira): Remove when all internal users switch to dateStyle.
      var dt = settings['dateType'];
      if (!/^(short|medium|long|full)$/.test(dt)) dt = 'short';
      cleanSettings['dateStyle'] = dt;
    }

    if (settings.hasOwnProperty('timeStyle')) {
      var ts = settings['timeStyle'];
      if (!/^(short|medium|long|full)$/.test(ts)) ts = 'short';
      cleanSettings['timeStyle'] = ts;
    } else if (settings.hasOwnProperty('timeType')) {
      // TODO(cira): Remove when all internal users switch to timeStyle.
      var tt = settings['timeType'];
      if (!/^(short|medium|long|full)$/.test(tt)) tt = 'short';
      cleanSettings['timeStyle'] = tt;
    }
  }

  // Default is to show short date and time.
  if (!cleanSettings.hasOwnProperty('skeleton') &&
      !cleanSettings.hasOwnProperty('dateStyle') &&
      !cleanSettings.hasOwnProperty('timeStyle')) {
    cleanSettings = {'dateStyle': 'short',
                     'timeStyle': 'short'};
  }

  locale = v8Locale.__createLocaleOrDefault(locale);
  var formatter = NativeJSDateTimeFormat(locale.__icuLocaleID, cleanSettings);

  // NativeJSDateTimeFormat creates formatter.options for us, we just need
  // to append actual settings to it.
  for (key in cleanSettings) {
    formatter.options[key] = cleanSettings[key];
  }

  /**
   * Clones existing date time format with possible overrides for some
   * of the options.
   * @param {!Object} overrideSettings - overrides for current format settings.
   * @returns {Object} - new DateTimeFormat object.
   * @public
   */
  formatter.derive = function(overrideSettings) {
    // To remove a setting user can specify undefined as its value. We'll remove
    // it from the map in that case.
    for (var prop in overrideSettings) {
      if (settings.hasOwnProperty(prop) && !overrideSettings[prop]) {
        delete settings[prop];
      }
    }
    return new v8Locale.__DateTimeFormat(
        locale, v8Locale.__createSettingsOrDefault(overrideSettings, settings));
  };

  return formatter;
};

/**
 * Creates new DateTimeFormat based on current locale.
 * @param {Object} - formatting flags. See constructor.
 * @returns {Object} - new DateTimeFormat object.
 */
v8Locale.prototype.createDateTimeFormat = function(settings) {
  return new v8Locale.__DateTimeFormat(this, settings);
};

/**
 * NumberFormat class implements locale-aware number formatting.
 * Constructor is not part of public API.
 * @param {Object} locale - locale object to pass to formatter.
 * @param {Object} settings - formatting flags:
 *   - skeleton
 *   - pattern
 *   - style - decimal, currency, percent or scientific
 *   - currencyCode - ISO 4217 3-letter currency code
 * @private
 * @constructor
 */
v8Locale.__NumberFormat = function(locale, settings) {
  native function NativeJSNumberFormat();

  settings = v8Locale.__createSettingsOrDefault(settings, {});

  var cleanSettings = {};
  if (settings.hasOwnProperty('skeleton')) {
    // Assign skeleton to cleanSettings and fix invalid currency pattern
    // if present - 'ooxo' becomes 'o'.
    cleanSettings['skeleton'] =
        settings['skeleton'].replace(/\u00a4+[^\u00a4]+\u00a4+/g, '\u00a4');
  } else if (settings.hasOwnProperty('pattern')) {
    cleanSettings['pattern'] = settings['pattern'];
  } else if (settings.hasOwnProperty('style')) {
    var style = settings['style'];
    if (!/^(decimal|currency|percent|scientific)$/.test(style)) {
      style = 'decimal';
    }
    cleanSettings['style'] = style;
  }

  // Default is to show decimal style.
  if (!cleanSettings.hasOwnProperty('skeleton') &&
      !cleanSettings.hasOwnProperty('pattern') &&
      !cleanSettings.hasOwnProperty('style')) {
    cleanSettings = {'style': 'decimal'};
  }

  // Add currency code if available and valid (3-letter ASCII code).
  if (settings.hasOwnProperty('currencyCode') &&
      /^[a-zA-Z]{3}$/.test(settings['currencyCode'])) {
    cleanSettings['currencyCode'] = settings['currencyCode'].toUpperCase();
  }

  locale = v8Locale.__createLocaleOrDefault(locale);
  // Pass in region ID for proper currency detection. Use ZZ if region is empty.
  var region = locale.options.regionID !== '' ? locale.options.regionID : 'ZZ';
  var formatter = NativeJSNumberFormat(
      locale.__icuLocaleID, 'und_' + region, cleanSettings);

  // ICU doesn't always uppercase the currency code.
  if (formatter.options.hasOwnProperty('currencyCode')) {
    formatter.options['currencyCode'] =
        formatter.options['currencyCode'].toUpperCase();
  }

  for (key in cleanSettings) {
    // Don't overwrite keys that are alredy in.
    if (formatter.options.hasOwnProperty(key)) continue;

    formatter.options[key] = cleanSettings[key];
  }

  /**
   * Clones existing number format with possible overrides for some
   * of the options.
   * @param {!Object} overrideSettings - overrides for current format settings.
   * @returns {Object} - new or cached NumberFormat object.
   * @public
   */
  formatter.derive = function(overrideSettings) {
    // To remove a setting user can specify undefined as its value. We'll remove
    // it from the map in that case.
    for (var prop in overrideSettings) {
      if (settings.hasOwnProperty(prop) && !overrideSettings[prop]) {
        delete settings[prop];
      }
    }
    return new v8Locale.__NumberFormat(
        locale, v8Locale.__createSettingsOrDefault(overrideSettings, settings));
  };

  return formatter;
};

/**
 * Creates new NumberFormat based on current locale.
 * @param {Object} - formatting flags. See constructor.
 * @returns {Object} - new or cached NumberFormat object.
 */
v8Locale.prototype.createNumberFormat = function(settings) {
  return new v8Locale.__NumberFormat(this, settings);
};

/**
 * Merges user settings and defaults.
 * Settings that are not of object type are rejected.
 * Actual property values are not validated, but whitespace is trimmed if they
 * are strings.
 * @param {!Object} settings - user provided settings.
 * @param {!Object} defaults - default values for this type of settings.
 * @returns {Object} - valid settings object.
 * @private
 */
v8Locale.__createSettingsOrDefault = function(settings, defaults) {
  if (!settings || typeof(settings) !== 'object' ) {
    return defaults;
  }
  for (var key in defaults) {
    if (!settings.hasOwnProperty(key)) {
      settings[key] = defaults[key];
    }
  }
  // Clean up settings.
  for (var key in settings) {
    // Trim whitespace.
    if (typeof(settings[key]) === "string") {
      settings[key] = settings[key].trim();
    }
    // Remove all properties that are set to undefined/null. This allows
    // derive method to remove a setting we don't need anymore.
    if (!settings[key]) {
      delete settings[key];
    }
  }

  return settings;
};

/**
 * If locale is valid (defined and of v8Locale type) we return it. If not
 * we create default locale and return it.
 * @param {!Object} locale - user provided locale.
 * @returns {Object} - v8Locale object.
 * @private
 */
v8Locale.__createLocaleOrDefault = function(locale) {
  if (!locale || !(locale instanceof v8Locale)) {
    return new v8Locale();
  } else {
    return locale;
  }
};
