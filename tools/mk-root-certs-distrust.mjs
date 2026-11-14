#!/usr/bin/env node
// Generates src/node_root_certs_distrust.h from tools/certdata.txt.
//
// Mozilla keeps some roots in its store but stops trusting TLS certificates
// they issue after a given date (CKA_NSS_SERVER_DISTRUST_AFTER). tools/
// mk-ca-bundle.pl does not carry that attribute into src/node_root_certs.h,
// so this script extracts it for every root that is in the bundle and emits
// the entries of the kDistrustAfterBundled table consumed by
// src/crypto/crypto_context.cc: the SHA-256 of the root's
// SubjectPublicKeyInfo and the cutoff as UTC seconds since the epoch.
//
// Run it after tools/mk-ca-bundle.pl, or let
// tools/dep_updaters/update-root-certs.mjs do both.
import { X509Certificate, createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { basename, join, relative } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';

const __filename = fileURLToPath(import.meta.url);
const checkoutDir = join(__filename, '..', '..');

const { values } = parseArgs({
  options: {
    help: { type: 'boolean' },
    certdata: { type: 'string', default: join(checkoutDir, 'tools', 'certdata.txt') },
    bundle: { type: 'string', default: join(checkoutDir, 'src', 'node_root_certs.h') },
    output: { short: 'o', type: 'string', default: join(checkoutDir, 'src', 'node_root_certs_distrust.h') },
    verbose: { short: 'v', type: 'boolean' },
  },
});

if (values.help) {
  console.log(`Usage: ${basename(__filename)} [OPTION]...`);
  console.log();
  console.log('Generates src/node_root_certs_distrust.h from tools/certdata.txt.');
  console.log('');
  console.log('      --certdata=FILE  certdata.txt to read (default: tools/certdata.txt)');
  console.log('      --bundle=FILE    node_root_certs.h to match against');
  console.log('                         (default: src/node_root_certs.h)');
  console.log('  -o, --output=FILE    header to write');
  console.log('                         (default: src/node_root_certs_distrust.h)');
  console.log('  -v, --verbose        writes progress to stdout');
  console.log('      --help           display this help and exit');
  process.exit(0);
}

// Decodes a MULTILINE_OCTAL value ("\062\064...") into a Buffer.
function decodeOctal(lines) {
  const bytes = [];
  for (const line of lines) {
    for (const m of line.matchAll(/\\([0-7]{3})/g)) {
      bytes.push(parseInt(m[1], 8));
    }
  }
  return Buffer.from(bytes);
}

// Reads a text file with line endings normalized to LF, so that a checkout
// with core.autocrlf cannot change the parse or the generated output.
function readText(file) {
  return readFileSync(file, 'utf8').replaceAll('\r\n', '\n');
}

// Parses the PKCS#11 objects in certdata.txt. Returns one { attributes } per
// object, with MULTILINE_OCTAL values decoded into Buffers and everything
// else kept as the raw string after the type.
function parseCertdata(text) {
  const objects = [];
  let current;
  const lines = text.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (line.startsWith('#') || line.trim() === '') continue;
    const m = line.match(/^(CKA_[A-Z_]+)\s+(\S+)(?:\s+(.*))?$/);
    if (m === null) continue;
    const [, name, type, rest] = m;
    if (name === 'CKA_CLASS') {
      current = { attributes: {} };
      objects.push(current);
    }
    if (current === undefined) continue;
    if (type === 'MULTILINE_OCTAL') {
      const octal = [];
      for (;;) {
        i++;
        if (i >= lines.length) {
          throw new Error(`Unterminated ${name} value at end of certdata.txt`);
        }
        if (lines[i] === 'END') break;
        octal.push(lines[i]);
      }
      current.attributes[name] = decodeOctal(octal);
    } else if (type === 'UTF8') {
      current.attributes[name] =
        rest.replace(/^"(.*)"$/, '$1').replaceAll(/\\(["\\])/g, '$1');
    } else {
      current.attributes[name] = rest;
    }
  }
  return objects;
}

// Parses an NSS distrust-after value into seconds since the epoch. NSS writes
// it as UTCTime, e.g. "261116235959Z" for 2026-11-16 23:59:59 UTC; a
// GeneralizedTime with a four-digit year is accepted as well.
function parseDistrustAfter(buf) {
  const s = buf.toString('latin1');
  const m = s.match(/^(\d{2}|\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})Z$/);
  if (m === null) throw new Error(`Unrecognized distrust-after value: ${s}`);
  const [, y, mo, dd, hh, mi, ss] = m.slice(0, 7).map(Number);
  // UTCTime: two-digit years from 50 are 19xx, the rest 20xx (RFC 5280).
  const year = m[1].length === 2 ? (y >= 50 ? 1900 + y : 2000 + y) : y;
  const date = new Date(Date.UTC(year, mo - 1, dd, hh, mi, ss));
  // Date.UTC normalizes out-of-range fields (a 31st of February becomes the
  // 3rd of March) instead of failing, so check that nothing moved.
  if (date.getUTCFullYear() !== year || date.getUTCMonth() !== mo - 1 ||
      date.getUTCDate() !== dd || date.getUTCHours() !== hh ||
      date.getUTCMinutes() !== mi || date.getUTCSeconds() !== ss) {
    throw new Error(`Invalid distrust-after date: ${s}`);
  }
  return date.getTime() / 1000;
}

function isoDate(epoch) {
  return new Date(epoch * 1000).toISOString().replace('T', ' ').replace(/\.000Z$/, ' UTC');
}

// SHA-256 fingerprints of every certificate in node_root_certs.h, so that
// roots Mozilla ships but mk-ca-bundle.pl skips (for example for lack of a
// server-auth trust bit) do not end up in the table.
function bundledFingerprints(headerText) {
  const fingerprints = new Set();
  const pemRE = /-----BEGIN CERTIFICATE-----\\n"\n([\s\S]*?)"-----END CERTIFICATE-----/g;
  for (const m of headerText.matchAll(pemRE)) {
    const base64 = m[1].replaceAll(/\\n"|"|\n/g, '');
    const der = Buffer.from(base64, 'base64');
    fingerprints.add(createHash('sha256').update(der).digest('hex'));
  }
  return fingerprints;
}

// Formats a digest the way clang-format lays out the hand-written entries in
// src/crypto/crypto_context.cc: eleven bytes per line.
function hexArray(buf) {
  const bytes = [...buf].map((b) => `0x${b.toString(16).padStart(2, '0')}`);
  const lines = [];
  for (let i = 0; i < bytes.length; i += 11) {
    lines.push((i === 0 ? '    {{' : '      ') + bytes.slice(i, i + 11).join(', ') + ',');
  }
  return lines.join('\n').replace(/,$/, '},');
}

const certdataFile = values.certdata;
const bundleFile = values.bundle;
const outputFile = values.output;

if (values.verbose) {
  console.log(`Reading ${certdataFile}`);
  console.log(`Reading ${bundleFile}`);
}
const bundled = bundledFingerprints(readText(bundleFile));
if (bundled.size === 0) {
  throw new Error(`No certificates found in ${bundleFile}`);
}
const objects = parseCertdata(readText(certdataFile));

const entries = [];
for (const { attributes } of objects) {
  if (attributes.CKA_CLASS !== 'CKO_CERTIFICATE') continue;
  const distrustAfter = attributes.CKA_NSS_SERVER_DISTRUST_AFTER;
  if (!Buffer.isBuffer(distrustAfter)) continue;  // Absent or CK_FALSE.
  const label = attributes.CKA_LABEL ?? '(unlabeled certificate)';
  const der = attributes.CKA_VALUE;
  if (!Buffer.isBuffer(der)) {
    throw new Error(`${label} has a distrust-after date but no CKA_VALUE`);
  }
  const fingerprint = createHash('sha256').update(der).digest('hex');
  if (!bundled.has(fingerprint)) {
    if (values.verbose) {
      console.log(`Skipping: ${label} is not in ${relative(checkoutDir, bundleFile)}`);
    }
    continue;
  }
  const cert = new X509Certificate(der);
  const spki = cert.publicKey.export({ type: 'spki', format: 'der' });
  // Only the label and the fingerprint go into the comment. The subject as
  // formatted by X509Certificate depends on the OpenSSL version, which would
  // make the output differ between Node.js builds for no reason.
  const entry = {
    label,
    fingerprint,
    spkiHash: createHash('sha256').update(spki).digest(),
    cutoff: parseDistrustAfter(distrustAfter),
  };
  entries.push(entry);
  if (values.verbose) {
    console.log(`Distrust after ${isoDate(entry.cutoff)}: ${label}`);
  }
}
// Code point order, so the output does not depend on ICU data.
entries.sort((a, b) => (a.label < b.label ? -1 : a.label > b.label ? 1 : 0));

const out = [
  '// This file is generated by tools/mk-root-certs-distrust.mjs from',
  '// tools/certdata.txt. Do not edit it by hand; rerun the script instead.',
  '//',
  '// Each entry is a root in node_root_certs.h that Mozilla keeps in its store',
  '// but distrusts for TLS server certificates issued after the given date',
  '// (CKA_NSS_SERVER_DISTRUST_AFTER): the SHA-256 of the root\'s',
  '// SubjectPublicKeyInfo and the last valid second as UTC seconds since the',
  '// epoch. The entries initialize kDistrustAfterBundled in',
  '// src/crypto/crypto_context.cc.',
  '',
  '#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS',
  '',
];
for (const entry of entries) {
  out.push(`/* ${entry.label.replaceAll('*/', '* /')} */`);
  out.push(`// Distrust after ${isoDate(entry.cutoff)}`);
  out.push(`// ${entry.fingerprint}`);
  out.push(hexArray(entry.spkiHash));
  out.push(`     ${entry.cutoff}},`);
  out.push('');
}
if (entries.length === 0) {
  out.push('// No bundled root carries a distrust-after date.');
  out.push('');
}
out.push('#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS');
out.push('');

const content = out.join('\n');
writeFileSync(outputFile, content);
if (values.verbose) {
  console.log(`Wrote ${entries.length} entries to ${outputFile}`);
}
