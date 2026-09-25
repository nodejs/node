'use strict';

// PGO Training Script: JSON Processing Workload
//
// JSON.parse() and JSON.stringify() are among the most frequently called
// functions in Node.js applications (every REST API request/response cycle).
// This script exercises the JSON parser and serializer with:
// - Small objects (API responses, config)
// - Medium objects (paginated lists, user profiles)
// - Large objects (analytics payloads, data exports)
// - Nested structures (deep objects, arrays of objects)
// - Various value types (strings, numbers, booleans, null, arrays, objects)
// - Edge cases (unicode, escaped characters, large numbers, empty objects)
// - Realistic patterns: parse → transform → stringify pipeline
//
// This exercises: V8 JSON parser, V8 JSON serializer, string allocation,
// property access patterns, array iteration, GC pressure from allocations.

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;
const ITERATIONS_PER_BATCH = 500;

// Realistic JSON payloads matching common API shapes

// 1. Small config/health check (~100 bytes)
const SMALL_PAYLOADS = [
  '{"status":"ok","uptime":12345,"version":"1.0.0"}',
  '{"id":42,"name":"test","active":true,"score":98.5}',
  '{"error":null,"data":{"key":"value"},"meta":{}}',
  '{"token":"abc123def456","expires":1700000000}',
  '{"success":true,"count":0,"items":[]}',
];

// 2. Medium user/product objects (~1-5 KB)
function generateUserPayload(count) {
  const users = [];
  for (let i = 0; i < count; i++) {
    users.push({
      id: `usr_${i.toString(36).padStart(8, '0')}`,
      email: `user${i}@company.example.com`,
      name: { first: `First${i}`, last: `Last${i}` },
      role: ['admin', 'editor', 'viewer', 'member'][i % 4],
      permissions: ['read', 'write', 'delete', 'admin'].slice(0, (i % 4) + 1),
      settings: {
        theme: i % 2 === 0 ? 'dark' : 'light',
        language: ['en', 'es', 'fr', 'de', 'ja'][i % 5],
        notifications: { email: true, push: i % 3 !== 0, sms: false },
        timezone: 'America/New_York',
      },
      metadata: {
        createdAt: '2024-01-15T08:30:00.000Z',
        updatedAt: '2024-12-01T14:22:33.456Z',
        lastLogin: '2024-12-15T09:45:00.000Z',
        loginCount: 100 + i * 7,
      },
    });
  }
  return JSON.stringify({ data: users, total: count, page: 1, perPage: count });
}

// 3. Large analytics/event payloads (~50-200 KB)
function generateEventPayload(count) {
  const events = [];
  for (let i = 0; i < count; i++) {
    events.push({
      id: `evt_${Date.now()}_${i}`,
      type: [
        'page_view',
        'click',
        'form_submit',
        'api_call',
        'error',
        'purchase',
      ][i % 6],
      timestamp: new Date(Date.now() - i * 60000).toISOString(),
      session: `sess_${Math.floor(i / 10)}`,
      user: i % 5 === 0 ? null : `usr_${i % 100}`,
      properties: {
        url: `https://example.com/page/${i % 20}?ref=dashboard&utm_source=email`,
        title: `Page Title ${i % 20} — Application Dashboard`,
        referrer: i % 3 === 0 ? 'https://google.com/search?q=test' : '',
        duration: Math.round(Math.random() * 30000),
        viewport: { width: 1920, height: 1080 },
        device: {
          type: ['desktop', 'mobile', 'tablet'][i % 3],
          os: ['Windows 11', 'macOS 14', 'iOS 17', 'Android 14'][i % 4],
          browser: ['Chrome 120', 'Firefox 121', 'Safari 17'][i % 3],
        },
      },
      context: {
        ip: `10.${i % 256}.${(i * 3) % 256}.${(i * 7) % 256}`,
        locale: ['en-US', 'en-GB', 'es-ES', 'fr-FR', 'de-DE'][i % 5],
        campaign:
          i % 10 === 0 ?
            { name: 'holiday_sale', medium: 'email', source: 'mailchimp' } :
            null,
      },
    });
  }
  return JSON.stringify({
    events,
    batch_id: `batch_${Date.now()}`,
    sent_at: new Date().toISOString(),
  });
}

// 4. Deeply nested structure (config files, GraphQL responses)
function generateNestedPayload(depth, breadth) {
  function nest(d) {
    if (d === 0) return { value: 'leaf', count: 42, tags: ['a', 'b'] };
    const obj = {};
    for (let i = 0; i < breadth; i++) {
      obj[`level_${d}_key_${i}`] = nest(d - 1);
    }
    return obj;
  }
  return JSON.stringify({ root: nest(depth), _meta: { depth, breadth } });
}

// 5. Array-heavy payload (table data, CSV-like)
function generateTablePayload(rows, cols) {
  const headers = Array.from({ length: cols }, (_, i) => `column_${i}`);
  const data = [];
  for (let r = 0; r < rows; r++) {
    const row = {};
    for (let c = 0; c < cols; c++) {
      row[headers[c]] =
        c % 3 === 0 ? r * c : c % 3 === 1 ? `val_${r}_${c}` : r % 2 === 0;
    }
    data.push(row);
  }
  return JSON.stringify({ headers, data, rowCount: rows });
}

// 6. Unicode-heavy payload (i18n content)
const UNICODE_PAYLOAD = JSON.stringify({
  messages: {
    en: { greeting: 'Hello, World!', farewell: 'Goodbye!' },
    ja: { greeting: 'こんにちは世界！', farewell: 'さようなら！' },
    ko: { greeting: '안녕하세요 세계!', farewell: '안녕히 가세요!' },
    zh: { greeting: '你好世界！', farewell: '再见！' },
    ar: { greeting: 'مرحبا بالعالم!', farewell: 'مع السلامة!' },
    ru: { greeting: 'Привет мир!', farewell: 'До свидания!' },
    de: { greeting: 'Hallo Welt!', farewell: 'Auf Wiedersehen!' },
    emoji: { greeting: '👋🌍✨', farewell: '👋😢💫' },
  },
  descriptions: Array.from(
    { length: 50 },
    (_, i) =>
      `Item ${i}: Ünîcödé tëst with spëcîal chars — «quotes» "double" 'single' & ampersand \\ backslash / slash`,
  ),
});

// Pre-generate payloads as strings
const mediumPayload10 = generateUserPayload(10);
const mediumPayload50 = generateUserPayload(50);
const largePayload100 = generateEventPayload(100);
const largePayload500 = generateEventPayload(500);
const nestedPayload = generateNestedPayload(5, 3);
const tablePayload = generateTablePayload(200, 15);

// All payloads weighted by real-world frequency
const PAYLOADS = [
  // Small payloads (health checks, simple responses) - most frequent
  ...SMALL_PAYLOADS.map((p) => ({ json: p, weight: 5 })),
  // Medium payloads (typical API responses)
  { json: mediumPayload10, weight: 8 },
  { json: mediumPayload50, weight: 6 },
  // Large payloads (analytics, batch operations)
  { json: largePayload100, weight: 3 },
  { json: largePayload500, weight: 1 },
  // Nested payloads (config, GraphQL)
  { json: nestedPayload, weight: 2 },
  // Table payloads (data grids, reports)
  { json: tablePayload, weight: 2 },
  // Unicode payloads (i18n)
  { json: UNICODE_PAYLOAD, weight: 2 },
];

const weightedPayloads = [];
for (const p of PAYLOADS) {
  for (let i = 0; i < p.weight; i++) {
    weightedPayloads.push(p.json);
  }
}

// Workload 1: Parse → access properties → stringify (REST API middleware pattern)
function workloadParseTransformStringify(jsonStr) {
  const obj = JSON.parse(jsonStr);

  // Typical middleware transforms
  if (obj.data && Array.isArray(obj.data)) {
    // Add computed field (common API pattern)
    for (const item of obj.data) {
      item._computed =
        typeof item.id === 'string' ? item.id.toUpperCase() : String(item.id);
    }
  }
  if (obj.events && Array.isArray(obj.events)) {
    // Filter/transform (analytics pipeline)
    obj.events = obj.events.filter((e) => e.type !== 'error');
    obj.filteredCount = obj.events.length;
  }

  return JSON.stringify(obj);
}

// Workload 2: Parse → extract subset → stringify (GraphQL resolver pattern)
function workloadSelectiveSerialize(jsonStr) {
  const obj = JSON.parse(jsonStr);
  const subset = {};

  // Pick only requested fields (like GraphQL field selection)
  for (const key of Object.keys(obj)) {
    if (typeof obj[key] !== 'object' || obj[key] === null) {
      subset[key] = obj[key];
    } else if (Array.isArray(obj[key]) && obj[key].length > 0) {
      subset[key] = obj[key].slice(0, 5).map((item) => {
        if (typeof item === 'object' && item !== null) {
          const { id, name, type, email } = item;
          return { id, name, type, email };
        }
        return item;
      });
    }
  }

  return JSON.stringify(subset);
}

// Workload 3: Repeated parse/stringify of same shape (template response caching pattern)
function workloadRepeatedShapes() {
  const results = [];
  for (let i = 0; i < 100; i++) {
    const obj = {
      id: i,
      name: `Item ${i}`,
      description: `Description for item ${i} with some additional text`,
      price: (i * 1.5).toFixed(2),
      inStock: i % 3 !== 0,
      categories: ['cat1', 'cat2'],
      ratings: { average: 4.5, count: i * 10 },
    };
    results.push(JSON.stringify(obj));
  }
  // Parse them all back
  return results.map((s) => JSON.parse(s));
}

// Workload 4: JSON.parse with reviver / JSON.stringify with replacer
function workloadReviverReplacer(jsonStr) {
  // Parse with date reviver (common pattern)
  const dateReviver = (key, value) => {
    if (typeof value === 'string' && /^\d{4}-\d{2}-\d{2}T/.test(value)) {
      return new Date(value);
    }
    return value;
  };
  const obj = JSON.parse(jsonStr, dateReviver);

  // Stringify with replacer to exclude nulls (sanitization)
  const nullRemover = (key, value) => (value === null ? undefined : value);
  return JSON.stringify(obj, nullRemover);
}

// Workload 5: Build large JSON incrementally (logging, audit trail)
function workloadIncrementalBuild() {
  const entries = [];
  for (let i = 0; i < 200; i++) {
    entries.push({
      level: ['info', 'warn', 'error', 'debug'][i % 4],
      message: `Log entry ${i}: Operation completed successfully with result code ${i * 3}`,
      timestamp: Date.now() - i * 1000,
      context: { requestId: `req_${i}`, userId: `user_${i % 20}` },
    });
  }
  const logBatch = JSON.stringify({ entries, count: entries.length });
  // Verify round-trip
  const parsed = JSON.parse(logBatch);
  return parsed.count;
}

async function main() {
  console.log('[pgo-json] Starting JSON processing workload...');
  const startTime = Date.now();
  let totalOps = 0;
  let batchNum = 0;

  while (Date.now() - startTime < DURATION_MS) {
    batchNum++;

    // Workload 1: Parse-transform-stringify (highest weight — most common)
    for (let i = 0; i < ITERATIONS_PER_BATCH; i++) {
      const payload = weightedPayloads[i % weightedPayloads.length];
      workloadParseTransformStringify(payload);
      totalOps++;
    }

    // Workload 2: Selective serialization
    for (let i = 0; i < Math.floor(ITERATIONS_PER_BATCH / 2); i++) {
      const payload = weightedPayloads[i % weightedPayloads.length];
      workloadSelectiveSerialize(payload);
      totalOps++;
    }

    // Workload 3: Repeated shapes (every 3rd batch)
    if (batchNum % 3 === 0) {
      workloadRepeatedShapes();
      totalOps += 200; // 100 stringify + 100 parse
    }

    // Workload 4: Reviver/replacer (every 2nd batch)
    if (batchNum % 2 === 0) {
      for (let i = 0; i < 50; i++) {
        const payload = weightedPayloads[i % weightedPayloads.length];
        workloadReviverReplacer(payload);
        totalOps++;
      }
    }

    // Workload 5: Incremental build (every 5th batch)
    if (batchNum % 5 === 0) {
      workloadIncrementalBuild();
      totalOps += 201; // 200 items + 1 round-trip
    }

    // Yield to event loop periodically (realistic for servers)
    if (batchNum % 10 === 0) {
      await new Promise((resolve) => setImmediate(resolve));
    }
  }

  const elapsed = (Date.now() - startTime) / 1000;
  console.log(
    `[pgo-json] Completed ${totalOps} JSON operations in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s)`,
  );
}

main().catch((err) => {
  console.error('[pgo-json] Error:', err);
  process.exit(1);
});
