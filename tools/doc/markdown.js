'use strict';

const visit = require('unist-util-visit');

module.exports = {
  replaceLinks
};

function replaceLinks() {
  return (tree) => {
    const linksIdenfitiers = tree.children
      .filter((child) => child.type === 'definition')
      .map((child) => child.identifier);

    visit(tree, 'linkReference', (node) => {
      const htmlLinkIdentifier = `${node.identifier} \`.html\``;
      if (linksIdenfitiers.includes(htmlLinkIdentifier)) {
        node.identifier = htmlLinkIdentifier;
      }
    });
  };
}
