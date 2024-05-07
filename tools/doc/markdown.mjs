import { visit } from 'unist-util-visit';

export const referenceToLocalMdFile = /^(?![+a-z]+:)([^#?]+)\.md(#.+)?$/i;
export const referenceToLocalMdFileInPre = /<a href="([^#"]+)\.md(#[^"]+)?">/g;

export function replaceLinks({ filename, linksMapper }) {
  return (tree) => {
    const fileHtmlUrls = linksMapper[filename];

    visit(tree, (node) => {
      if (node.url) {
        node.url = node.url.replace(
          referenceToLocalMdFile,
          (_, filename, hash) => `${filename}.html${hash || ''}`,
        );
      }
    });
    visit(tree, 'code', (node) => {
      if (node.meta === 'html') {
        node.value = node.value.replace(
          referenceToLocalMdFileInPre,
          (_, path, fragment) => `<a href="${path}.html${fragment || ''}">`,
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
