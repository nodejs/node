import { visit } from 'unist-util-visit';

export const referenceToLocalMdFile = /^(?![+a-z]+:)([^#?]+)\.md(#.+)?$/i;

export default () => {
  return (tree) => {
    visit(tree, (node) => {
      node.url &&= node.url.replace(
        referenceToLocalMdFile,
        (_, filename, hash) => `${filename}.html${hash || ''}`,
      );
    });
  };
};
