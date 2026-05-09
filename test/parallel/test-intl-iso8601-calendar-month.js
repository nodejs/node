'use strict';

// Regression test for nodejs/node#63041:
// `Intl.DateTimeFormat` with `calendar: 'iso8601'` previously dropped the
// month-name part because ICU's iso8601 calendar resource bundle is empty
// and inherits blank month-name symbols instead of falling back to gregory.
// The fix routes pattern lookup through ca=gregory while keeping the
// iso8601 Calendar instance attached, so resolvedOptions().calendar still
// reports "iso8601" and the formatted output contains the month name.

const common = require('../common');
if (!common.hasIntl) {
  common.skip('missing Intl');
}

const assert = require('assert');

const date = new Date('2024-09-09T08:00:00Z');

{
  // dateStyle:'full' + timeStyle:'long' is the exact case from the bug report.
  const dtf = new Intl.DateTimeFormat('en-US', {
    dateStyle: 'full',
    timeStyle: 'long',
    timeZone: 'UTC',
    calendar: 'iso8601',
  });
  const formatted = dtf.format(date);
  assert.match(
    formatted,
    /September/,
    `expected month name in ${JSON.stringify(formatted)}`,
  );
  // resolvedOptions().calendar must still be the user-requested iso8601.
  assert.strictEqual(dtf.resolvedOptions().calendar, 'iso8601');
}

{
  // Explicit field options must also retain the month name.
  const dtf = new Intl.DateTimeFormat('en-US', {
    weekday: 'long',
    year: 'numeric',
    month: 'long',
    day: 'numeric',
    timeZone: 'UTC',
    calendar: 'iso8601',
  });
  assert.match(dtf.format(date), /September/);
  assert.strictEqual(dtf.resolvedOptions().calendar, 'iso8601');
}

{
  // Standalone month: 'long' must format as the month name, not empty string.
  const dtf = new Intl.DateTimeFormat('en-US', {
    month: 'long',
    timeZone: 'UTC',
    calendar: 'iso8601',
  });
  assert.strictEqual(dtf.format(date), 'September');
}

{
  // dateStyle:'short' has always produced ISO-style numeric output and should
  // continue to do so.
  const dtf = new Intl.DateTimeFormat('en-US', {
    dateStyle: 'short',
    timeZone: 'UTC',
    calendar: 'iso8601',
  });
  assert.strictEqual(dtf.format(date), '2024-09-09');
}

{
  // formatToParts must expose the month part with the iso8601 calendar.
  const dtf = new Intl.DateTimeFormat('en-US', {
    dateStyle: 'long',
    timeZone: 'UTC',
    calendar: 'iso8601',
  });
  const parts = dtf.formatToParts(date);
  const monthPart = parts.find((p) => p.type === 'month');
  assert.ok(monthPart, `expected a month part, got ${JSON.stringify(parts)}`);
  assert.strictEqual(monthPart.value, 'September');
}
