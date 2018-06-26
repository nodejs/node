'use strict';

// Build all.html by combining the generated toc and apicontent from each
// of the generated html files.

const fs = require('fs');
const { JSDOM } = require('jsdom');

const source = `${__dirname}/../../out/doc/api`;

const html_files = fs.readdirSync(source, 'utf8')
  .filter((name) => name.includes('.html') && name !== 'all.html');

const all = {};
for (const file of html_files) {
  all[file] = new JSDOM(fs.readFileSync(source + '/' + file, 'utf8'))
    .window.document;
}

const doc = all['_toc.html'].documentElement;
const list = Array.from(doc.querySelectorAll('#column2 ul li a'))
  .map((node) => node.getAttribute('href'));

const canonical = doc.querySelector('link[rel=canonical]');
canonical.setAttribute('href',
                       canonical.getAttribute('href').replace('_toc', 'all'));

doc.querySelector('body').setAttribute('id', 'api-section-all');

const owner = doc.ownerDocument;
const toc = doc.querySelector('#toc');
const ul = owner.createElement('ul');
toc.appendChild(ul);
const api = doc.querySelector('#apicontent');
while (api.firstChild) api.firstChild.remove();

const title = doc.querySelector('title');
title.textContent = title.textContent.replace(/.* \| /, '');
doc.querySelector('#column1').setAttribute('data-id', 'all');
doc.querySelector('a[href="_toc.json"]').setAttribute('href', 'all.json');

for (const file of list) {
  if (!all[file]) continue;

  let source = all[file].querySelector('#toc ul');
  while (source.firstChild) ul.appendChild(owner.adoptNode(source.firstChild));

  source = all[file].querySelector('#apicontent');
  while (source.firstChild) api.appendChild(owner.adoptNode(source.firstChild));
}

const output = `<!doctype html>\n${doc.outerHTML}`;
fs.writeFileSync(source + '/all.html', output, 'utf8');
