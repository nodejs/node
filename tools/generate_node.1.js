'use strict';

const { resolve, join } = require('node:path');
const { readFileSync, writeFileSync } = require('node:fs');

const HEADER = `
.\\"
.\\" This manpage is written in mdoc(7).
.\\"
.\\" * Language reference:
.\\"   https://man.openbsd.org/mdoc.7
.\\"
.\\" * Linting changes:
.\\"   mandoc -Wall -Tlint /path/to/this.file  # BSD
.\\"   groff -w all -z /path/to/this.file      # GNU/Linux, macOS
.\\"
.\\"
.\\" Before making changes, please note the following:
.\\"
.\\" * In Roff, each new sentence should begin on a new line. This gives
.\\"   the Roff formatter better control over text-spacing, line-wrapping,
.\\"   and paragraph justification.
.\\"
.\\" * Do not leave blank lines in the markup. If whitespace is desired
.\\"   for readability, put a dot in the first column to indicate a null/empty
.\\"   command. Comments and horizontal whitespace may optionally follow: each
.\\"   of these lines are an example of a null command immediately followed by
.\\"   a comment.
.\\"
.\\"======================================================================
.
.tr -\\-^\\(ha~\\(ti\`\\(ga
.Dd 2018
.Dt NODE 1
.
.Sh NAME
.Nm node
.Nd server-side JavaScript runtime
.
.\\"======================================================================
.Sh SYNOPSIS
.Nm node
.Op Ar options
.Op Ar v8-options
.Op Fl e Ar string | Ar script.js | Fl
.Op Fl -
.Op Ar arguments ...
.
.Nm node
.Cm inspect
.Op Fl e Ar string | Ar script.js | Fl | Ar <host>:<port>
.Ar ...
.
.Nm node
.Op Fl -v8-options
.
.\\"======================================================================
.Sh DESCRIPTION
Node.js is a set of libraries for JavaScript which allows it to be used outside of the browser.
It is primarily focused on creating simple, easy-to-build network clients and servers.
.Pp
Execute
.Nm
without arguments to start a REPL.
.
.Sh OPTIONS
.Bl -tag -width 6n
.It Sy -
Alias for stdin, analogous to the use of - in other command-line utilities.
The executed script is read from stdin, and remaining arguments are passed to the script.
.
.It Fl -
Indicate the end of command-line options.
Pass the rest of the arguments to the script.
.Pp
If no script filename or eval/print script is supplied prior to this, then
the next argument will be used as a script filename.
.
`.trimStart();

const ROOT = resolve(__dirname, '..');
const CLI_MD = readFileSync(join(ROOT, 'doc', 'api', 'cli.md'), 'utf-8');

// Error message constant
const ERROR_MSG = 'Could not find sentence.';

// Function to extract the first sentence from the text
const extractFirstSentence = (lines, startLineIndex) => {
  const text = lines.slice(startLineIndex).join('\n');
  let isInsideQuotes = false;

  for (let i = 0; i < text.length; i++) {
    if (text[i] === '.') {
      if (i + 1 < text.length && (text[i + 1] === ' ' || text[i + 1] === '\n')) {
        // Toggle quote state
        for (let j = i - 1; j >= 0; j--) {
          if (['"', '`'].includes(text[j])) {
            isInsideQuotes = !isInsideQuotes;
          } else if (text[j] === "'" && text[j - 1] === ' ') {
            isInsideQuotes = !isInsideQuotes;
          } else if (text[j] === '\\') {
            j--;
          }
        }
        if (!isInsideQuotes) {
          return text.slice(0, i + 1).trim();
        }
      }
    }
  }

  throw new Error(ERROR_MSG);
};

// Function to parse individual lines in the markdown file
const parseLine = (line, index, allLines) => {
  const matched = line.match(/^### (.+)/);
  if (!matched) return;

  const option = matched[1].replace(/`/g, '');
  if (option === '-' || option === '--') return;

  // Check for HTML comment and skip to end of comment if found
  if (allLines[index + 2]?.startsWith('<!--')) {
    index = allLines.findIndex((el, idx) => idx > index && el === '-->');
  } else if (allLines[index + 2]?.startsWith('### ')) {
    return { option, description: 'This option has no description.' };
  }

  let descriptionLine = '';
  while (!descriptionLine || descriptionLine[0] === '>') {
    descriptionLine = allLines[++index];
  }

  let description = replaceMarkdown(extractFirstSentence(allLines, index));
  if (description[0].match(/[^A-Za-z0-9"']/)) description = 'This option has no description.';
  return { option, description };
};

// Function to replace markdown formatting
const replaceMarkdown = (input) => input
    .replace(/\*\*(.*?)\*\*/g, '.B $1')
    .replace(/\[(.*?)\][[(].*?[\])]/g, '$1')
    .replace(/`/g, '"')
    .replace(/`(["'])/g, '$1').replace(/(["'])`/g, '$1');

// Function to get command line options
const getOptions = () => {
  const options = { flags: [], envVars: [] };
  const lines = CLI_MD.split('\n');
  let currentHeader;

  for (const line of lines) {
    if (line.startsWith('## ')) {
      currentHeader = line.substring(3);
    } else if (['Options', 'Environment variables', 'Useful V8 options'].includes(currentHeader)) {
      const parsedLine = parseLine(line, lines.indexOf(line), lines);
      if (parsedLine) {
        const targetArray = (currentHeader === 'Options' || currentHeader === 'Useful V8 options') ?
          options.flags :
          options.envVars;
        targetArray.push(parsedLine);
      }
    }
  }

  // Remove duplicates and sort flags
  options.flags = [...new Map(options.flags.map((item) => [item.option, item])).values()]
    .sort((a, b) => a.option.localeCompare(b.option));

  return options;
};

const { envVars, flags } = getOptions();

// Function to extract value from the flag
const applyValue = (flag) => {
  const [, flagValue] = flag.split(/[= ]/);
  const sep = flag.match(/[=\s]/)?.[0];
  return sep === ' ' ? '' : flagValue ? ` Ns ${sep} Ns Ar ${flagValue.replace(/\]$/, '')}` : '';
};

// Function to create flag section
const createFlagSection = () => flags.map(({ option, description }) => {
  const flagMandoc = option.split(', ').map((flag) => {
    const [innerFlag] = flag.split(/\[?[= ]/);
    return `Fl ${innerFlag.slice(1)}${applyValue(flag)}`;
  }).join(' , ');

  return `.It ${flagMandoc.trim()}\n${description}`;
}).join('\n.\n') + '\n.\n';

// Function to create environment variable section
const createEnvVarSection = () => envVars.map(({ option, description }) => {
  const [varName, varValue] = option.split('=');
  const value = varValue ? ` Ar ${varValue}` : '';
  return `.It Ev ${varName}${value}\n${description}`;
}).join('\n.\n') + '\n.\n';

// Footer section
const FOOTER = `
.El
.\\"=====================================================================
.Sh BUGS
Bugs are tracked in GitHub Issues:
.Sy https://github.com/nodejs/node/issues
.
.\\"======================================================================
.Sh COPYRIGHT
Copyright Node.js contributors.
Node.js is available under the MIT license.
.
.Pp
Node.js also includes external libraries that are available under a variety of licenses.
See
.Sy https://github.com/nodejs/node/blob/HEAD/LICENSE
for the full license text.
.
.\\"======================================================================
.Sh SEE ALSO
Website:
.Sy https://nodejs.org/
.
.Pp
Documentation:
.Sy https://nodejs.org/api/
.
.Pp
GitHub repository and issue tracker:
.Sy https://github.com/nodejs/node
`.trim();

const ENV_HEADER = `
.El
.
.\\" =====================================================================
.Sh ENVIRONMENT
.Bl -tag -width 6n
`.trimStart();

// Write the final content to the output file
writeFileSync(
  join(ROOT, 'doc', 'node.1'),
  HEADER + createFlagSection() + ENV_HEADER + createEnvVarSection() + FOOTER + '\n',
);
