'use strict';

const visit = require('unist-util-visit');

const referenceToLocalMdFile = /^(?![+a-z]+:)([^#?]+)\.md(#.+)?$/i;

module.exports = {
  replaceLinks,
  referenceToLocalMdFile,
};

function replaceLinks({ filename, linksMapper }) {
  return (tree) => {
    const fileHtmlUrls = linksMapper[filename];

    visit(tree, (node) => {
      if (node.url) {
        node.url = node.url.replace(
          referenceToLocalMdFile,
          (_, filename, hash) => `${filename}.html${hash || ''}`
        );
      }
    });
    visit(tree, 'definition', (node) => {
      const htmlUrl = fileHtmlUrls && fileHtmlUrls[node.identifier];

      if (htmlUrl && typeof htmlUrl === 'string') {
        node.url = htmlUrl;
      }
    });
  };
}
