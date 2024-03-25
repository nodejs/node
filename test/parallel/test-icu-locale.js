'use strict';
const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

if (!common.hasIntl)
  common.skip('Intl not present.');

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=fr-FR',
    '-p',
    'Intl?.Collator().resolvedOptions().locale',
  ],
  {
    signal: null,
    stdout: /fr-FR/,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=de-DE',
    '-p',
    'Intl?.Collator().resolvedOptions().locale',
  ],
  {
    signal: null,
    stdout: /de-DE/,
  });

// NumberFormat
spawnSyncAndExitWithoutError(
  process.execPath,
  ['--icu-locale=de-DE', '-p', '(123456.789).toLocaleString(undefined, { style: "currency", currency: "EUR" })'],
  {
    signal: null,
    stdout: /123\.456,79\xa0€/u,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=jp-JP',
    '-p',
    '(123456.789).toLocaleString(undefined, { style: "currency", currency: "JPY" })',
  ],
  {
    signal: null,
    stdout: /JP¥\xa0123,457/u,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=en-IN',
    '-p',
    '(123456.789).toLocaleString(undefined, { maximumSignificantDigits: 3  })',
  ],
  {
    signal: null,
    stdout: /1,23,000/u,
  });

// DateTimeFormat
spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=en-GB',
    '-p',
    'new Date(Date.UTC(2012, 11, 20, 3, 0, 0)).toLocaleString(undefined,  { timeZone: "UTC" })',
  ],
  {
    signal: null,
    stdout: /20\/12\/2012, 03:00:00/u,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=ko-KR',
    '-p',
    'new Date(Date.UTC(2012, 11, 20, 3, 0, 0)).toLocaleString(undefined,  { timeZone: "UTC" })',
  ],
  {
    signal: null,
    stdout: /2012\. 12\. 20\. 오전 3:00:00/u,
  });

// ListFormat
spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=en-US',
    '-p',
    'new Intl.ListFormat(undefined, {style: "long", type:"conjunction"}).format(["a", "b", "c"])',
  ],
  {
    signal: null,
    stdout: /a, b, and c/u,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=de-DE',
    '-p',
    'new Intl.ListFormat(undefined, {style: "long", type:"conjunction"}).format(["a", "b", "c"])',
  ],
  {
    signal: null,
    stdout: /a, b und c/u,
  });

// RelativeTimeFormat
spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=en',
    '-p',
    'new Intl.RelativeTimeFormat(undefined, { numeric: "auto" }).format(3, "quarter")',
  ],
  {
    signal: null,
    stdout: /in 3 quarters/u,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=es',
    '-p',
    'new Intl.RelativeTimeFormat(undefined, { numeric: "auto" }).format(2, "day")',
  ],
  {
    signal: null,
    stdout: /pasado mañana/u,
  });

// Collator

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=de',
    '-p',
    '["Z", "a", "z", "ä"].sort(new Intl.Collator().compare).join(",")',
  ],
  {
    signal: null,
    stdout: /a,ä,z,Z/u,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=sv',
    '-p',
    '["Z", "a", "z", "ä"].sort(new Intl.Collator().compare).join(",")',
  ],
  {
    signal: null,
    stdout: /a,z,Z,ä/u,
  });

spawnSyncAndExitWithoutError(
  process.execPath,
  [
    '--icu-locale=de',
    '-p',
    '["Z", "a", "z", "ä"].sort(new Intl.Collator(undefined, { caseFirst: "upper" }).compare).join(",")',
  ],
  {
    signal: null,
    stdout: /a,ä,Z,z/u,
  });
