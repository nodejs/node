# Relative Date

[![Build Status](https://travis-ci.org/wildlyinaccurate/tiny-relative-date.png?branch=master)](https://travis-ci.org/wildlyinaccurate/tiny-relative-date)

Tiny function that provides relative, human-readable dates.

## Installation

```
npm install tiny-relative-date
```

## Usage

The module returns a `relativeDate` function with English translations by default.

```js
const relativeDate = require('tiny-relative-date')
```

The `relativeDate` function accepts date strings or `Date` objects.

```js
relativeDate('2017-06-25 09:00') // '12 hours ago'
relativeDate(new Date()) // 'just now'
```

The value of "now" can also be passed as a second parameter.

```js
const now = new Date('2017-06-25 08:00:00')
const date = new Date('2017-06-25 07:00:00')

relativeDate(date, now) // 'an hour ago'
```

### Using a non-English locale

The tiny-relative-date module can be initialised with a locale. See the [translations directory]('./translations') for a list of available locales.

```js
const relativeDateFactory = require('tiny-relative-date/lib/factory')
const deTranslations = require('tiny-relative-date/translations/de')
const relativeDate = relativeDateFactory(deTranslations)

relativeDate(new Date()) // 'gerade eben'
```

### Using a custom locale

You can also use a completely custom locale by passing a translations object instead of a locale string. Translations can be plain strings with a `{{time}}` placeholder, or they can be functions. See the **Adding new locales** section below for a list of translation keys.

```js
const relativeDateFactory = require('tiny-relative-date/lib/factory')
const relativeDate = relativeDateFactory({
  hoursAgo: '{{time}}h ago',
  daysAgo: (days) => `${days * 24}h ago`
})

relativeDate('2017-06-25 07:00:00') // '2h ago'
relativeDate('2017-06-24 06:00:00') // '27h ago'
```

## Contributing

Contributions are welcome! Running this project locally requires Git and Node.js.

```
git clone git@github.com:wildlyinaccurate/tiny-relative-date.git
cd tiny-relative-date/
npm install
```

Once you are set up, you can make changes to files in the `src/`, `spec/` and `translations/` directories. Build any changes you make by running

```
npm run build
```

And run the tests with

```
npm run test
```

### Adding new locales

If you would like to add a new locale, please create a JSON file in the `translations` directory and ensure it has the following keys:

| Key                    | Default value ("en" locale) |
|------------------------|-----------------------------|
| `justNow`             | just now                    |
| `secondsAgo`          | {{time}} seconds ago        |
| `aMinuteAgo`         | a minute ago                |
| `minutesAgo`          | {{time}} minutes ago        |
| `anHourAgo`          | an hour ago                 |
| `hoursAgo`            | {{time}} hours ago          |
| `aDayAgo`            | yesterday                   |
| `daysAgo`             | {{time}} days ago           |
| `aWeekAgo`           | a week ago                  |
| `weeksAgo`            | {{time}} weeks ago          |
| `aMonthAgo`          | a month ago                 |
| `monthsAgo`           | {{time}} months ago         |
| `aYearAgo`           | a year ago                  |
| `yearsAgo`            | {{time}} years ago          |
| `overAYearAgo`      | over a year ago             |
| `secondsFromNow`     | {{time}} seconds from now   |
| `aMinuteFromNow`    | a minute from now           |
| `minutesFromNow`     | {{time}} minutes from now   |
| `anHourFromNow`     | an hour from now            |
| `hoursFromNow`       | {{time}} hours from now     |
| `aDayFromNow`       | tomorrow                    |
| `daysFromNow`        | {{time}} days from now      |
| `aWeekFromNow`      | a week from now             |
| `weeksFromNow`       | {{time}} weeks from now     |
| `aMonthFromNow`     | a month from now            |
| `monthsFromNow`      | {{time}} months from now    |
| `aYearFromNow`      | a year from now             |
| `yearsFromNow`       | {{time}} years from now     |
| `overAYearFromNow` | over a year from now        |
