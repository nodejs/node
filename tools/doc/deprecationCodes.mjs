import fs from 'fs';
import { resolve } from 'path';
import assert from 'assert';

import { unified } from 'unified';
import remarkParse from 'remark-parse';

const source = resolve(process.argv[2]);

const skipDeprecationComment = /^<!-- md-lint skip-deprecation (DEP\d{4}) -->$/;

const generateDeprecationCode = (codeAsNumber) =>
  `DEP${codeAsNumber.toString().padStart(4, '0')}`;

const addMarkdownPathToErrorStack = (error, node) => {
  const { line, column } = node.position.start;
  const [header, ...lines] = error.stack.split('\n');
  error.stack =
          header +
          `\n    at <anonymous> (${source}:${line}:${column})\n` +
          lines.join('\n');
  return error;
};

const testHeading = (headingNode, expectedDeprecationCode) => {
  try {
    assert.strictEqual(
      headingNode?.children[0]?.value.substring(0, 9),
      `${expectedDeprecationCode}: `,
      'Ill-formed or out-of-order deprecation code.'
    );
  } catch (e) {
    throw addMarkdownPathToErrorStack(e, headingNode);
  }
};

const testYAMLComment = (commentNode) => {
  try {
    assert.match(
      commentNode?.value?.substring(0, 21),
      /^<!-- YAML\r?\nchanges:\r?\n/,
      'Missing or ill-formed YAML comment.'
    );
  } catch (e) {
    throw addMarkdownPathToErrorStack(e, commentNode);
  }
};

const testDeprecationType = (paragraphNode) => {
  try {
    assert.strictEqual(
      paragraphNode?.children[0]?.value?.substring(0, 6),
      'Type: ',
      'Missing deprecation type.'
    );
  } catch (e) {
    throw addMarkdownPathToErrorStack(e, paragraphNode);
  }
};

const tree = unified()
  .use(remarkParse)
  .parse(fs.readFileSync(source));

let expectedDeprecationCodeNumber = 0;
for (let i = 0; i < tree.children.length; i++) {
  const node = tree.children[i];
  if (node.type === 'html' && skipDeprecationComment.test(node.value)) {
    const expectedDeprecationCode =
          generateDeprecationCode(++expectedDeprecationCodeNumber);
    const deprecationCodeAsText = node.value.match(skipDeprecationComment)[1];

    try {
      assert.strictEqual(
        deprecationCodeAsText,
        expectedDeprecationCode,
        'Deprecation codes are not ordered correctly.'
      );
    } catch (e) {
      throw addMarkdownPathToErrorStack(e, node);
    }
  }
  if (node.type === 'heading' && node.depth === 3) {
    const expectedDeprecationCode =
          generateDeprecationCode(++expectedDeprecationCodeNumber);

    testHeading(node, expectedDeprecationCode);

    testYAMLComment(tree.children[i + 1]);
    testDeprecationType(tree.children[i + 2]);
  }
}
