// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import express from 'express';
import fs from 'fs';
import path from 'path';
import { marked } from 'marked';
import { Graphviz } from '@hpcc-js/wasm-graphviz';

const app = express();
const PORT = 8081;

app.use(express.json());

// Resolve paths
const __dirname = path.resolve(); // Cwd is docs/viewer
const docsDir = path.resolve(__dirname, '..');
const distDir = path.resolve(__dirname, 'dist');

console.log('Loading Graphviz WASM...');
const g = await Graphviz.load();

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

// Helper to render markdown to HTML with Graphviz
async function renderMarkdown(filePath, relPath) {
  let content = fs.readFileSync(filePath, 'utf-8');

  // Handle Graphviz blocks
  const codeBlockRegex = /```(?:graphviz|dot)\n([\s\S]*?)\n```/g;
  let match;
  const replacements = [];

  while ((match = codeBlockRegex.exec(content)) !== null) {
    let dot = match[1];
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
      console.error(`Failed to render graphviz:`, e);
      replacements.push({
        original: match[0],
        replacement: `<div class="graphviz"><pre>${e.message}</pre></div>`
      });
    }
  }

  for (const r of replacements) {
    content = content.replace(r.original, r.replacement);
  }

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

  return html;
}

// Serve static files for the viewer itself
app.use('/viewer', express.static(__dirname));

// Handle requests for processed HTML files
app.get('*', async (req, res, next) => {
  const relPath = req.path.slice(1); // Remove leading slash

  if (!relPath.endsWith('.html')) {
    return next();
  }

  const htmlPath = path.join(distDir, relPath);
  const mdPath = path.join(docsDir, relPath.replace(/\.html$/, '.md'));

  try {
    if (fs.existsSync(htmlPath)) {
      res.sendFile(htmlPath);
    } else if (fs.existsSync(mdPath)) {
      console.log(`Generating on the fly: ${relPath}`);
      const html = await renderMarkdown(mdPath, relPath);
      res.send(html);
    } else {
      res.status(404).send('File not found');
    }
  } catch (error) {
    res.status(500).send(`Error: ${error.message}`);
  }
});

// Handle comments API
const commentsFile = path.resolve(__dirname, 'comments.json');

function readComments() {
  if (fs.existsSync(commentsFile)) {
    try {
      return JSON.parse(fs.readFileSync(commentsFile, 'utf-8'));
    } catch (e) {
      return {};
    }
  }
  return {};
}

app.get('/api/comments', (req, res) => {
  const file = req.query.file;
  const comments = readComments();
  res.json(comments[file] || {});
});

app.post('/api/comments', (req, res) => {
  const { file, blockIndex, text } = req.body;

  if (!file || blockIndex === undefined || !text) {
    return res.status(400).send('Bad Request: Missing fields');
  }

  const comments = readComments();

  if (!comments[file]) comments[file] = {};
  const blockIndexStr = String(blockIndex);
  if (!comments[file][blockIndexStr]) comments[file][blockIndexStr] = [];

  comments[file][blockIndexStr].push({
    text,
    timestamp: new Date().toISOString()
  });

  fs.writeFileSync(commentsFile, JSON.stringify(comments, null, 2));
  res.json({ success: true });
});

// Default route serves index.html
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'index.html'));
});

app.listen(PORT, () => {
  console.log(`Server running at http://localhost:${PORT}/`);
  console.log(`Viewer available at http://localhost:${PORT}/viewer/`);
});
