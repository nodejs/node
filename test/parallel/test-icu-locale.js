'use strict';
const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const assert = require('assert');

if (!common.hasIntl)
  common.skip('Intl not present.');
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=fr-FR',
      '-p',
      'Intl?.Collator().resolvedOptions().locale',
    ]);
  assert.strictEqual(child.stdout.toString(), 'fr-FR\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=en-GB',
      '-p',
      'Intl?.Collator().resolvedOptions().locale',
    ]);
  assert.strictEqual(child.stdout.toString(), 'en-GB\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=en_GB',
      '-p',
      'Intl?.Collator().resolvedOptions().locale',
    ]);
  assert.strictEqual(child.stdout.toString(), 'en-GB\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=en',
      '-p',
      'Intl?.Collator().resolvedOptions().locale',
    ]);
  assert.strictEqual(child.stdout.toString(), 'en\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=de-DE',
      '-p',
      'Intl?.Collator().resolvedOptions().locale',
    ]);
  assert.strictEqual(child.stdout.toString(), 'de-DE\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=invalid',
      '-p',
      'Intl?.Collator().resolvedOptions().locale',
    ]);
  assert.strictEqual(child.stdout.toString(), 'en-US\n');
}

// NumberFormat
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    ['--icu-locale=de-DE', '-p', '(123456.789).toLocaleString(undefined, { style: "currency", currency: "EUR" })']
  );
  assert.strictEqual(child.stdout.toString(), '123.456,79\xa0€\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=ja-JP',
      '-p',
      '(123456.789).toLocaleString(undefined, { style: "currency", currency: "JPY" })',
    ]);
  assert.strictEqual(child.stdout.toString(), '￥123,457\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=en-IN',
      '-p',
      '(123456.789).toLocaleString(undefined, { maximumSignificantDigits: 3  })',
    ]);
  assert.strictEqual(child.stdout.toString(), '1,23,000\n');
}
// DateTimeFormat
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=en-GB',
      '-p',
      'new Date(Date.UTC(2012, 11, 20, 3, 0, 0)).toLocaleString(undefined,  { timeZone: "UTC" })',
    ]);
  assert.strictEqual(child.stdout.toString(), '20/12/2012, 03:00:00\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=ko-KR',
      '-p',
      'new Date(Date.UTC(2012, 11, 20, 3, 0, 0)).toLocaleString(undefined,  { timeZone: "UTC" })',
    ]);
  assert.strictEqual(child.stdout.toString(), '2012. 12. 20. 오전 3:00:00\n');
}

// ListFormat
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=en-US',
      '-p',
      'new Intl.ListFormat(undefined, {style: "long", type:"conjunction"}).format(["a", "b", "c"])',
    ]);
  assert.strictEqual(child.stdout.toString(), 'a, b, and c\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=de-DE',
      '-p',
      'new Intl.ListFormat(undefined, {style: "long", type:"conjunction"}).format(["a", "b", "c"])',
    ]);
  assert.strictEqual(child.stdout.toString(), 'a, b und c\n');
}

// RelativeTimeFormat
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=en',
      '-p',
      'new Intl.RelativeTimeFormat(undefined, { numeric: "auto" }).format(3, "quarter")',
    ]);
  assert.strictEqual(child.stdout.toString(), 'in 3 quarters\n');
}
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=es',
      '-p',
      'new Intl.RelativeTimeFormat(undefined, { numeric: "auto" }).format(2, "day")',
    ]);
  assert.strictEqual(child.stdout.toString(), 'pasado mañana\n');
}

// Collator
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=de',
      '-p',
      '["Z", "a", "z", "ä"].sort(new Intl.Collator().compare).join(",")',
    ]);
  assert.strictEqual(child.stdout.toString(), 'a,ä,z,Z\n');
}
{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=sv',
      '-p',
      '["Z", "a", "z", "ä"].sort(new Intl.Collator().compare).join(",")',
    ]);
  assert.strictEqual(child.stdout.toString(), 'a,z,Z,ä\n');
}

{
  const { child } = spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--icu-locale=de',
      '-p',
      '["Z", "a", "z", "ä"].sort(new Intl.Collator(undefined, { caseFirst: "upper" }).compare).join(",")',
    ]);
  assert.strictEqual(child.stdout.toString(), 'a,ä,Z,z\n');
}
