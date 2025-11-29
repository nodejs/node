'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = relativeDateFactory;
var calculateDelta = function calculateDelta(now, date) {
  return Math.round(Math.abs(now - date) / 1000);
};

function relativeDateFactory(translations) {
  return function relativeDate(date) {
    var now = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : new Date();

    if (!(date instanceof Date)) {
      date = new Date(date);
    }

    var delta = null;

    var minute = 60;
    var hour = minute * 60;
    var day = hour * 24;
    var week = day * 7;
    var month = day * 30;
    var year = day * 365;

    delta = calculateDelta(now, date);

    if (delta > day && delta < week) {
      date = new Date(date.getFullYear(), date.getMonth(), date.getDate(), 0, 0, 0);
      delta = calculateDelta(now, date);
    }

    var translate = function translate(translatePhrase, timeValue, rawValue) {
      var key = void 0;

      if (translatePhrase === 'justNow') {
        key = translatePhrase;
      } else if (now >= date) {
        key = translatePhrase + 'Ago';
      } else {
        key = translatePhrase + 'FromNow';
      }

      var translation = translations[key];

      if (typeof translation === 'function') {
        return translation(timeValue, rawValue);
      }

      return translation.replace('{{time}}', timeValue);
    };

    switch (false) {
      case !(delta < 30):
        return translate('justNow', delta, delta);

      case !(delta < minute):
        return translate('seconds', delta, delta);

      case !(delta < 2 * minute):
        return translate('aMinute', 1, delta);

      case !(delta < hour):
        return translate('minutes', Math.floor(delta / minute), delta);

      case Math.floor(delta / hour) !== 1:
        return translate('anHour', Math.floor(delta / minute), delta);

      case !(delta < day):
        return translate('hours', Math.floor(delta / hour), delta);

      case !(delta < day * 2):
        return translate('aDay', 1, delta);

      case !(delta < week):
        return translate('days', Math.floor(delta / day), delta);

      case Math.floor(delta / week) !== 1:
        return translate('aWeek', 1, delta);

      case !(delta < month):
        return translate('weeks', Math.floor(delta / week), delta);

      case Math.floor(delta / month) !== 1:
        return translate('aMonth', 1, delta);

      case !(delta < year):
        return translate('months', Math.floor(delta / month), delta);

      case Math.floor(delta / year) !== 1:
        return translate('aYear', 1, delta);

      default:
        return translate('overAYear', Math.floor(delta / year), delta);
    }
  };
}
module.exports = exports['default'];