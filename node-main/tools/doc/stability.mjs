// Build stability table to documentation.html/json/md by generated all.json

import fs from 'fs';

import raw from 'rehype-raw';
import htmlStringify from 'rehype-stringify';
import gfm from 'remark-gfm';
import markdown from 'remark-parse';
import remark2rehype from 'remark-rehype';
import { unified } from 'unified';
import { visit } from 'unist-util-visit';

const source = new URL('../../out/doc/api/', import.meta.url);
const data = JSON.parse(fs.readFileSync(new URL('./all.json', source), 'utf8'));
const markBegin = '<!-- STABILITY_OVERVIEW_SLOT_BEGIN -->';
const markEnd = '<!-- STABILITY_OVERVIEW_SLOT_END -->';
const mark = `${markBegin}(.*)${markEnd}`;

const output = {
  json: new URL('./stability.json', source),
  docHTML: new URL('./documentation.html', source),
  docJSON: new URL('./documentation.json', source),
  docMarkdown: new URL('./documentation.md', source),
};

function collectStability(data) {
  const stability = [];

  for (const mod of data.modules) {
    if (mod.displayName && mod.stability >= 0) {
      const link = mod.source.replace('doc/api/', '').replace('.md', '.html');

      let { stabilityText } = mod;
      if (stabilityText.includes('. ')) {
        stabilityText = stabilityText.slice(0, stabilityText.indexOf('.'));
      }

      stability.push({
        api: mod.name,
        displayName: mod.textRaw,
        link: link,
        stability: mod.stability,
        stabilityText: `(${mod.stability}) ${stabilityText}`,
      });
    }
  }

  stability.sort((a, b) => a.displayName.localeCompare(b.displayName));
  return stability;
}

function createMarkdownTable(data) {
  const md = ['| API | Stability |', '| --- | --------- |'];

  for (const mod of data) {
    md.push(`| [${mod.displayName}](${mod.link}) | ${mod.stabilityText} |`);
  }

  return md.join('\n');
}

function createHTML(md) {
  const file = unified()
    .use(markdown)
    .use(gfm)
    .use(remark2rehype, { allowDangerousHtml: true })
    .use(raw)
    .use(htmlStringify)
    .use(processStability)
    .processSync(md);

  return file.value.trim();
}

function processStability() {
  return (tree) => {
    visit(tree, { type: 'element', tagName: 'tr' }, (node) => {
      const [api, stability] = node.children;
      if (api.tagName !== 'td') {
        return;
      }

      api.properties.class = 'module_stability';

      const level = stability.children[0].value[1];
      stability.properties.class = `api_stability api_stability_${level}`;
    });
  };
}

function updateStabilityMark(file, value, mark) {
  const fd = fs.openSync(file, 'r+');
  const content = fs.readFileSync(fd, { encoding: 'utf8' });

  const replaced = content.replace(mark, value);
  if (replaced !== content) {
    fs.writeSync(fd, replaced, 0, 'utf8');
  }
  fs.closeSync(fd);
}

const stability = collectStability(data);

// add markdown
const markdownTable = createMarkdownTable(stability);
updateStabilityMark(output.docMarkdown,
                    `${markBegin}\n${markdownTable}\n${markEnd}`,
                    new RegExp(mark, 's'));

// add html table
const html = createHTML(markdownTable);
updateStabilityMark(output.docHTML, `${markBegin}${html}${markEnd}`,
                    new RegExp(mark, 's'));

// add json output
updateStabilityMark(output.docJSON,
                    JSON.stringify(`${markBegin}${html}${markEnd}`),
                    new RegExp(JSON.stringify(mark), 's'));
