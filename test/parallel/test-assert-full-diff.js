// Flags: --full-assert-diff

'use strict';

require('../common');

const assert = require('assert');

{
  // Large Paragraph(s)
  const expected = `
    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
    Sit amet mattis vulputate enim nulla aliquet porttitor lacus luctus.
    Turpis egestas integer eget aliquet nibh praesent tristique magna.
    Malesuada bibendum arcu vitae elementum curabitur vitae.
    Feugiat vivamus at augue eget.
    Erat velit scelerisque in dictum non consectetur a erat.
    Id nibh tortor id aliquet.
    Blandit cursus risus at ultrices mi tempus.
    Varius morbi enim nunc faucibus a pellentesque sit.
    Quam quisque id diam vel quam.
    Sit amet mattis vulputate enim.
    Lectus urna duis convallis convallis tellus id interdum velit.
    Lacinia quis vel eros donec ac odio tempor orci dapibus.
    Purus non enim praesent elementum facilisis leo.
    Condimentum id venenatis a condimentum vitae. Pretium viverra suspendisse potenti nullam ac.
    `;
  const actual = expected.replace('Id nibh tortor id aliquet.', 'That\'s a tortor that trucks');

  try {
    assert.strictEqual(actual, expected);
  } catch (err) {
    assert.ok(expected.split('\n').every((line) => err.message.includes(line)));
    assert.ok(actual.split('\n').every((line) => err.message.includes(line)));
  }
}

{
  // Large Array
  const expected = Array.from({ length: 1000 }, (_, i) => i);
  const actual = Array.from({ length: 1000 }, (_, i) => i + (i === 500 ? 1 : 0));

  try {
    assert.deepStrictEqual(actual, expected);
  } catch (err) {
    assert.ok(expected.every((line) => err.message.includes(line)));
    assert.ok(actual.every((line) => err.message.includes(line)));
  }
}

{
  // Large Single-Line String
  const expected = 'A'.repeat(500) + 'B' + 'A'.repeat(500);
  const actual = expected.replace('B', 'C');

  try {
    assert.strictEqual(actual, expected);
  } catch (err) {
    assert.ok(err.message.includes(expected));
    assert.ok(err.message.includes(actual));
  }
}
