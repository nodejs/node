import '../common/index.mjs';

import fs from 'fs';
import assert from 'assert';

import {
  remarkParse,
  unified,
} from '../../tools/doc/deps.mjs';

const ignore = ['deprecations.md', 'documentation.md'];

const docURL = new URL('../../doc/api/', import.meta.url);
const docList = fs.readdirSync(docURL).filter((filename) => !ignore.includes(filename));

const re = /^Stability: \d/;

for (const file of docList) {
  const fileURL = new URL(file, docURL);
  const tree = unified()
    .use(remarkParse)
    .parse(fs.readFileSync(fileURL));

  // Traverse first-level nodes, ignoring comment blocks
  const nodes = tree.children.filter((node) => node.type !== 'html');

  for (let i = 0; i < nodes.length; i++) {
    const { [i]: node, [i - 1]: previousNode } = nodes;
    if (node.type !== 'blockquote' || !node.children.length) continue;

    const paragraph = node.children[0];
    if (paragraph.type !== 'paragraph' || !paragraph.children.length) continue;

    const text = paragraph.children[0];
    if (text.type !== 'text' || !re.exec(text.value)) continue;

    // Check that previous node type (excluding comment blocks) is one of:
    // * 'heading'
    // * 'paragraph' with a leading 'strong' node (pseudo-heading, eg. assert.equal)
    try {
      assert(previousNode.type === 'heading' ||
                 (previousNode.type === 'paragraph' && previousNode.children[0]?.type === 'strong'),
             'Stability block must be the first content element under heading');
    } catch (error) {
      const { line, column } = node.position.start;
      error.stack = error.stack.split('\n')
                      .toSpliced(1, 0, `    at ${fileURL}:${line}:${column}`)
                      .join('\n');
      throw error;
    }
  }
}
