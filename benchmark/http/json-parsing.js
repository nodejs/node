'use strict';

const common = require('../common.js');
const http = require('http');

const configs = {
  small: {
    json: JSON.stringify({
      id: 1,
      name: 'Alice',
      active: true,
      score: 9.5,
    }),
    schema: {
      type: 'object',
      properties: {
        id: { type: 'integer' },
        name: { type: 'string' },
        active: { type: 'boolean' },
        score: { type: 'number' },
      },
    },
  },
  medium: {
    json: JSON.stringify({
      order_id: 'ORD-12345',
      created_at: '2024-01-01T00:00:00Z',
      status: 'pending',
      total: 109.97,
      currency: 'USD',
      user: { id: 1, name: 'Alice', email: 'alice@example.com' },
      items: [
        { product_id: 'P1', name: 'Widget', quantity: 2, price: 29.99, sku: 'WGT-001' },
        { product_id: 'P2', name: 'Gadget', quantity: 1, price: 49.99, sku: 'GDG-001' },
      ],
      shipping_address: {
        street: '123 Main St',
        city: 'Anytown',
        state: 'CA',
        zip: '12345',
        country: 'US',
      },
    }),
    schema: {
      type: 'object',
      properties: {
        order_id: { type: 'string' },
        status: { type: 'string' },
        total: { type: 'number' },
        items: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              product_id: { type: 'string' },
              quantity: { type: 'integer' },
              price: { type: 'number' },
            },
          },
        },
      },
    },
  },
  large: {
    json: JSON.stringify({
      id: 'usr_abc123',
      username: 'alice_wonder',
      email: 'alice@example.com',
      full_name: 'Alice Wonderland',
      created_at: '2020-01-01T00:00:00Z',
      updated_at: '2024-06-15T12:30:00Z',
      last_login: '2024-06-20T08:00:00Z',
      is_active: true,
      is_verified: true,
      role: 'admin',
      plan: 'enterprise',
      preferences: {
        theme: 'dark',
        language: 'en',
        notifications: { email: true, sms: false, push: true },
        timezone: 'America/New_York',
        date_format: 'MM/DD/YYYY',
      },
      profile: {
        bio: 'Software Engineer',
        website: 'https://example.com',
        avatar_url: 'https://cdn.example.com/avatars/alice.jpg',
        location: 'New York, NY',
        company: 'Acme Corp',
        job_title: 'Senior Engineer',
        phone: '+1-555-0100',
      },
      stats: {
        total_orders: 42,
        total_spent: 1234.56,
        loyalty_points: 9876,
        reviews_count: 15,
        avg_rating: 4.8,
      },
      tags: ['vip', 'early-adopter', 'beta-tester'],
      addresses: [
        {
          id: 'addr_1',
          type: 'billing',
          street: '123 Main St',
          city: 'New York',
          state: 'NY',
          zip: '10001',
          country: 'US',
          is_default: true,
        },
        {
          id: 'addr_2',
          type: 'shipping',
          street: '456 Oak Ave',
          city: 'Brooklyn',
          state: 'NY',
          zip: '11201',
          country: 'US',
          is_default: false,
        },
      ],
      payment_methods: [
        { id: 'pm_1', type: 'credit_card', last4: '4242', brand: 'Visa', exp_month: 12, exp_year: 2027 },
        { id: 'pm_2', type: 'paypal', email: 'alice@paypal.com' },
      ],
    }),
    schema: {
      type: 'object',
      properties: {
        id: { type: 'string' },
        username: { type: 'string' },
        email: { type: 'string' },
        is_active: { type: 'boolean' },
        role: { type: 'string' },
      },
    },
  },
};

const bench = common.createBenchmark(main, {
  method: ['node', 'v8', 'ajv'],
  payload: Object.keys(configs),
  connections: [100, 500],
  duration: 5,
});

function main({ method, payload, connections, duration }) {
  const { json, schema } = configs[payload];

  let parser;
  let validate;
  if (method === 'node') {
    const { Parser } = require('node:json');
    parser = new Parser(schema);
    parser.parse(json);  // Warm up before accepting connections.
  } else if (method === 'ajv') {
    const Ajv = require('ajv');
    const ajv = new Ajv();
    validate = ajv.compile(schema);
    validate(JSON.parse(json));  // Warm up before accepting connections.
  }

  const response = Buffer.from('{"ok":true}');

  const server = http.createServer((_req, res) => {
    if (method === 'node') {
      parser.parse(json);
    } else if (method === 'ajv') {
      validate(JSON.parse(json));
    } else {
      JSON.parse(json);
    }

    res.writeHead(200, {
      'Content-Type': 'application/json',
      'Content-Length': response.length,
    });
    res.end(response);
  });

  server.listen(0, () => {
    bench.http({
      path: '/',
      connections,
      duration,
      port: server.address().port,
    }, () => {
      server.close();
    });
  });
}
