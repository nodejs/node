#!/usr/bin/env node

const path = require('path');
const fs = require('fs');
const yaml = require('yaml');
const cmark = require('cmark-gfm');
const mdx = require('@mdx-js/mdx');
const mkdirp = require('mkdirp');
const jsdom = require('jsdom');
const npm = require('../lib/npm.js')

const config = require('./config.json');

const docsRoot = __dirname;
const inputRoot = path.join(docsRoot, 'content');
const outputRoot = path.join(docsRoot, 'output');

const template = fs.readFileSync('template.html').toString();

const run = async function() {
    try {
        const navPaths = await getNavigationPaths();
        const fsPaths = await renderFilesystemPaths();

        if (!ensureNavigationComplete(navPaths, fsPaths)) {
            process.exit(1);
        }
    }
    catch (error) {
        console.error(error);
    }
}

run();

function ensureNavigationComplete(navPaths, fsPaths) {
    const unmatchedNav = { }, unmatchedFs = { };

    for (const navPath of navPaths) {
        unmatchedNav[navPath] = true;
    }

    for (let fsPath of fsPaths) {
        fsPath = '/' + fsPath.replace(/\.md$/, "");

        if (unmatchedNav[fsPath]) {
            delete unmatchedNav[fsPath];
        }
        else {
            unmatchedFs[fsPath] = true;
        }
    }

    const missingNav = Object.keys(unmatchedNav).sort();
    const missingFs = Object.keys(unmatchedFs).sort()

    if (missingNav.length > 0 || missingFs.length > 0) {
        let message = "Error: documentation navigation (nav.yml) does not match filesystem.\n";

        if (missingNav.length > 0) {
            message += "\nThe following path(s) exist on disk but are not present in nav.yml:\n\n";

            for (const nav of missingNav) {
                message += `  ${nav}\n`;
            }
        }

        if (missingNav.length > 0 && missingFs.length > 0) {
            message += "\nThe following path(s) exist in nav.yml but are not present on disk:\n\n";

            for (const fs of missingFs) {
                message += `  ${fs}\n`;
            }
        }

        message += "\nUpdate nav.yml to ensure that all files are listed in the appropriate place.";

        console.error(message);

        return false;
    }

    return true;
}

function getNavigationPaths() {
    const navFilename = path.join(docsRoot, 'nav.yml');
    const nav = yaml.parse(fs.readFileSync(navFilename).toString(), 'utf8');

    return walkNavigation(nav);
}

function walkNavigation(entries) {
    const paths = [ ]

    for (const entry of entries) {
        if (entry.children) {
            paths.push(... walkNavigation(entry.children));
        }
        else {
            paths.push(entry.url);
        }
    }

    return paths;
}

async function renderFilesystemPaths() {
    return await walkFilesystem(inputRoot);
}

async function walkFilesystem(root, dirRelative) {
    const paths = [ ]

    const dirPath = dirRelative ? path.join(root, dirRelative) : root;
    const children = fs.readdirSync(dirPath);

    for (const childFilename of children) {
        const childRelative = dirRelative ? path.join(dirRelative, childFilename) : childFilename;
        const childPath = path.join(root, childRelative);

        if (fs.lstatSync(childPath).isDirectory()) {
            paths.push(... await walkFilesystem(root, childRelative));
        }
        else {
            await renderFile(childRelative);
            paths.push(childRelative);
        }
    }

    return paths;
}

async function renderFile(childPath) {
    const inputPath = path.join(inputRoot, childPath);

    if (!inputPath.match(/\.md$/)) {
        console.log(`warning: unknown file type ${inputPath}, ignored`);
        return;
    }

    const outputPath = path.join(outputRoot, childPath.replace(/\.md$/, '.html'));

    let md = fs.readFileSync(inputPath).toString();
    let frontmatter = { };

    // Take the leading frontmatter out of the markdown
    md = md.replace(/^---\n([\s\S]+)\n---\n/, (header, fm) => {
        frontmatter = yaml.parse(fm, 'utf8');
        return '';
    });

    // Replace any tokens in the source
    md = md.replace(/@VERSION@/, npm.version);

    // Render the markdown into an HTML snippet using a GFM renderer.
    const content = cmark.renderHtmlSync(md, {
        'smart': true,
        'githubPreLang': true,
        'strikethroughDoubleTilde': true,
        'unsafe': false,
        extensions: {
            'table': true,
            'strikethrough': true,
            'tagfilter': true,
            'autolink': true
        }
    });

    // Test that mdx can parse this markdown file.  We don't actually
    // use the output, it's just to ensure that the upstream docs
    // site (docs.npmjs.com) can parse it when this file gets there.
    try {
        await mdx(md, { skipExport: true });
    }
    catch (error) {
        throw new MarkdownError(childPath, error);
    }

    // Inject this data into the template, using a mustache-like
    // replacement scheme.
    const html = template.replace(/\{\{\s*([\w\.]+)\s*\}\}/g, (token, key) => {
        switch (key) {
            case 'content':
                return `<div id="_content">${content}</div>`;
            case 'path':
                return childPath;
            case 'url_path':
                return encodeURI(childPath);

            case 'toc':
                return '<div id="_table_of_contents"></div>';

            case 'title':
            case 'section':
            case 'description':
                return frontmatter[key];

            case 'config.github_repo':
            case 'config.github_branch':
            case 'config.github_path':
                return config[key.replace(/^config\./, '')];

            default:
                console.log(`warning: unknown token '${token}' in ${inputPath}`);
                return '';
        }
        return key;
    });

    const dom = new jsdom.JSDOM(html);
    const document = dom.window.document;

    // Rewrite relative URLs in links and image sources to be relative to
    // this file; this is for supporting `file://` links.  HTML pages need
    // suffix appended.
    const links = [
        { tag: 'a', attr: 'href', suffix: '.html' },
        { tag: 'img', attr: 'src' }
    ];

    for (let linktype of links) {
        for (let tag of document.querySelectorAll(linktype.tag)) {
            let url = tag.getAttribute(linktype.attr);

            if (url.startsWith('/')) {
                const childDepth = childPath.split('/').length - 1;
                const prefix = childDepth > 0 ? '../'.repeat(childDepth) : './';

                url = url.replace(/^\//, prefix);

                if (linktype.suffix) {
                    url += linktype.suffix;
                }

                tag.setAttribute(linktype.attr, url);
            }
        }
    }

    // Give headers a unique id so that they can be linked within the doc
    const headerIds = [ ];
    for (let header of document.querySelectorAll('h1, h2, h3, h4, h5, h6')) {
        if (header.getAttribute('id')) {
            headerIds.push(header.getAttribute('id'));
            continue;
        }

        const headerText = header.textContent.replace(/[A-Z]/g, x => x.toLowerCase()).replace(/ /g, '-').replace(/[^a-z0-9\-]/g, '');
        let headerId = headerText;
        let headerIncrement = 1;

        while (document.getElementById(headerId) !== null) {
            headerId = headerText + (++headerIncrement);
        }

        headerIds.push(headerId);
        header.setAttribute('id', headerId);
    }

    // Walk the dom and build a table of contents
    const toc = document.getElementById('_table_of_contents');

    if (toc) {
        toc.appendChild(generateTableOfContents(document));
    }

    // Write the final output
    const output = dom.serialize();

    mkdirp.sync(path.dirname(outputPath));
    fs.writeFileSync(outputPath, output);
}

function generateTableOfContents(document) {
    const headers = [ ];
    walkHeaders(document.getElementById('_content'), headers);

    let parent = null;

    // The nesting depth of headers are not necessarily the header level.
    // (eg, h1 > h3 > h5 is a depth of three even though there's an h5.)
    const hierarchy = [ ];
    for (let header of headers) {
        const level = headerLevel(header);

        while (hierarchy.length && hierarchy[hierarchy.length - 1].headerLevel > level) {
            hierarchy.pop();
        }

        if (!hierarchy.length || hierarchy[hierarchy.length - 1].headerLevel < level) {
            const newList = document.createElement('ul');
            newList.headerLevel = level;

            if (hierarchy.length) {
                hierarchy[hierarchy.length - 1].appendChild(newList);
            }

            hierarchy.push(newList);
        }

        const element = document.createElement('li');

        const link = document.createElement('a');
        link.setAttribute('href', `#${header.getAttribute('id')}`);
        link.innerHTML = header.innerHTML;
        element.appendChild(link);

        const list = hierarchy[hierarchy.length - 1];
        list.appendChild(element);
    }

    return hierarchy[0];
}

function walkHeaders(element, headers) {
    for (let child of element.childNodes) {
        if (headerLevel(child)) {
            headers.push(child);
        }

        walkHeaders(child, headers);
    }
}

function headerLevel(node) {
    const level = node.tagName ? node.tagName.match(/^[Hh]([123456])$/) : null;
    return level ? level[1] : 0;
}

function debug(str) {
    console.log(str);
}

class MarkdownError extends Error {
    constructor(file, inner) {
        super(`failed to parse ${file}`);
        this.file = file;
        this.inner = inner;
    }
}
