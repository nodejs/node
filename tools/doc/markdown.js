'use strict';

const visit = require('unist-util-visit');

module.exports = {
  replaceLinks
};

function replaceLinks({ filename, linksMapper }) {
  return (tree) => {
    const fileHtmlUrls = linksMapper[filename];

    visit(tree, 'definition', (node) => {
      const htmlUrl = fileHtmlUrls && fileHtmlUrls[node.identifier];

      if (htmlUrl && typeof htmlUrl === 'string') {
        node.url = htmlUrl;
      }
    });
  };
}
