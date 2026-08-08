// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import fs from 'fs';
import path from 'path';
import { marked } from 'marked';
import { Graphviz } from '@hpcc-js/wasm-graphviz';

const docsDir = path.resolve('..'); // Parent directory is docs/
const outputDir = path.resolve('dist'); // output to docs/viewer/dist

function resolvePath(currentFile, relativePath) {
  if (relativePath.startsWith('/')) return relativePath.slice(1);

  const parts = currentFile.split('/');
  parts.pop(); // Remove filename

  const relParts = relativePath.split('/');
  for (const part of relParts) {
    if (part === '.') continue;
    if (part === '..') parts.pop();
    else parts.push(part);
  }
  return parts.join('/');
}

function generateSidebar(docsDir, outputDir) {
  const readmePath = path.join(docsDir, 'README.md');
  if (!fs.existsSync(readmePath)) {
    console.error('README.md not found, cannot generate sidebar.');
    return;
  }

  const content = fs.readFileSync(readmePath, 'utf-8');
  const lines = content.split('\n');

  const sidebar = [];
  let currentSection = null;
  let currentSubsection = null;

  for (const line of lines) {
    const sectionMatch = line.match(/^###\s+(.+)$/);
    if (sectionMatch) {
      currentSection = {
        title: sectionMatch[1],
        items: [],
        subsections: []
      };
      sidebar.push(currentSection);
      currentSubsection = null;
      continue;
    }

    const subsectionMatch = line.match(/^####\s+(.+)$/);
    if (subsectionMatch && currentSection) {
      if (subsectionMatch[1] === 'Common') {
        currentSubsection = null;
      } else {
        currentSubsection = {
          title: subsectionMatch[1],
          items: []
        };
        currentSection.subsections.push(currentSubsection);
      }
      continue;
    }

    const itemMatch = line.match(/^\*\s+\[([^\]]+)\]\(([^)]+)\)/);
    if (itemMatch) {
      const item = {
        title: itemMatch[1],
        file: itemMatch[2].replace(/\.md$/, '.html')
      };

      if (currentSubsection) {
        currentSubsection.items.push(item);
      } else if (currentSection) {
        currentSection.items.push(item);
      }
    }
  }

  const outputPath = path.join(outputDir, 'sidebar.json');
  fs.writeFileSync(outputPath, JSON.stringify(sidebar, null, 2));
  console.log('Generated sidebar.json');
}

async function run() {
  console.log('Loading Graphviz WASM...');
  const g = await Graphviz.load();

  async function processDir(dir, relativePath = '') {
    const entries = fs.readdirSync(dir, { withFileTypes: true });

    for (const entry of entries) {
      const fullPath = path.join(dir, entry.name);
      const relPath = path.join(relativePath, entry.name);

      if (entry.isDirectory()) {
        if (entry.name === 'viewer' || entry.name === 'node_modules' || entry.name === '.git') continue;
        await processDir(fullPath, relPath);
      } else if (entry.isFile() && entry.name.endsWith('.md')) {
        console.log(`Processing ${relPath}`);
        let content = fs.readFileSync(fullPath, 'utf-8');

        // Handle Graphviz blocks in markdown
        const codeBlockRegex = /```(?:graphviz|dot)\n([\s\S]*?)\n```/g;
        let match;
        const replacements = [];

        while ((match = codeBlockRegex.exec(content)) !== null) {
          let dot = match[1];
          // Inject default fontname similar to index.html
          const firstBrace = dot.indexOf('{');
          if (firstBrace !== -1) {
            dot = dot.slice(0, firstBrace + 1) +
              '\n    bgcolor="transparent";' +
              '\n    graph [fontname="Courier"];' +
              '\n    node [fontname="Courier"];' +
              '\n    edge [fontname="Courier"];' +
              dot.slice(firstBrace + 1);
          }

          try {
            const svg = g.layout(dot, "svg", "dot");
            const modifiedSvg = svg.replaceAll('font-family="Courier,monospace"', 'font-family="JetBrains Mono"');
            replacements.push({
              original: match[0],
              replacement: `<div class="graphviz">${modifiedSvg}</div>`
            });
          } catch (e) {
            console.error(`Failed to render graphviz in ${relPath}:`, e);
            replacements.push({
              original: match[0],
              replacement: `<div class="graphviz"><pre>${e.message}</pre></div>`
            });
          }
        }

        for (const r of replacements) {
          content = content.replace(r.original, r.replacement);
        }

        // Parse markdown to HTML
        let html = await marked.parse(content);

        // Rewrite links
        html = html.replaceAll(/href="([^"]+)"/g, (match, href) => {
          if (href.startsWith('http')) return match; // Skip external links

          const resolvedPath = resolvePath(relPath, href);

          if (href.startsWith('src/') || href.startsWith('include/') ||
              resolvedPath.startsWith('src/') || resolvedPath.startsWith('include/')) {

            const finalPath = href.startsWith('src/') || href.startsWith('include/') ? href : resolvedPath;
            return `href="https://source.chromium.org/chromium/chromium/src/+/main:v8/${finalPath}" target="_blank"`;
          }

          if (href.endsWith('.md')) {
            return `href="${href.replace(/\.md$/, '.html')}"`;
          }

          return match;
        });

        // Convert inline code paths to links
        html = html.replaceAll(/<code>((?:src|include)\/[^<]+)<\/code>/g, '<a href="https://source.chromium.org/chromium/chromium/src/+/main:v8/$1" target="_blank"><code>$1</code></a>');

        // Save to output dir
        const targetPath = path.join(outputDir, relPath.replace(/\.md$/, '.html'));
        fs.mkdirSync(path.dirname(targetPath), { recursive: true });
        fs.writeFileSync(targetPath, html);
      }
    }
  }

  fs.mkdirSync(outputDir, { recursive: true });
  generateSidebar(docsDir, outputDir);
  await processDir(docsDir);
  console.log('Generation complete.');
}

run().catch(console.error);
