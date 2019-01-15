'use strict';

const { types } = internalBinding('options');
const hasCrypto = Boolean(process.versions.openssl);

const typeLookup = [];
for (const key of Object.keys(types))
  typeLookup[types[key]] = key;

// Environment variables are parsed ad-hoc throughout the code base,
// so we gather the documentation here.
const { hasIntl, hasSmallICU, hasNodeOptions } = internalBinding('config');
const envVars = new Map([
  ['NODE_DEBUG', { helpText: "','-separated list of core modules that " +
    'should print debug information' }],
  ['NODE_DEBUG_NATIVE', { helpText: "','-separated list of C++ core debug " +
    'categories that should print debug output' }],
  ['NODE_DISABLE_COLORS', { helpText: 'set to 1 to disable colors in ' +
    'the REPL' }],
  ['NODE_EXTRA_CA_CERTS', { helpText: 'path to additional CA certificates ' +
    'file' }],
  ['NODE_NO_WARNINGS', { helpText: 'set to 1 to silence process warnings' }],
  ['NODE_PATH', { helpText: `'${require('path').delimiter}'-separated list ` +
    'of directories prefixed to the module search path' }],
  ['NODE_PENDING_DEPRECATION', { helpText: 'set to 1 to emit pending ' +
    'deprecation warnings' }],
  ['NODE_PRESERVE_SYMLINKS', { helpText: 'set to 1 to preserve symbolic ' +
    'links when resolving and caching modules' }],
  ['NODE_REDIRECT_WARNINGS', { helpText: 'write warnings to path instead ' +
    'of stderr' }],
  ['NODE_REPL_HISTORY', { helpText: 'path to the persistent REPL ' +
    'history file' }],
  ['NODE_TLS_REJECT_UNAUTHORIZED', { helpText: 'set to 0 to disable TLS ' +
    'certificate validation' }],
  ['NODE_V8_COVERAGE', { helpText: 'directory to output v8 coverage JSON ' +
    'to' }],
  ['UV_THREADPOOL_SIZE', { helpText: 'sets the number of threads used in ' +
    'libuv\'s threadpool' }]
].concat(hasIntl ? [
  ['NODE_ICU_DATA', { helpText: 'data path for ICU (Intl object) data' +
    hasSmallICU ? '' : ' (will extend linked-in data)' }]
] : []).concat(hasNodeOptions ? [
  ['NODE_OPTIONS', { helpText: 'set CLI options in the environment via a ' +
    'space-separated list' }]
] : []).concat(hasCrypto ? [
  ['OPENSSL_CONF', { helpText: 'load OpenSSL configuration from file' }],
  ['SSL_CERT_DIR', { helpText: 'sets OpenSSL\'s directory of trusted ' +
    'certificates when used in conjunction with --use-openssl-ca' }],
  ['SSL_CERT_FILE', { helpText: 'sets OpenSSL\'s trusted certificate file ' +
    'when used in conjunction with --use-openssl-ca' }],
] : []));


function indent(text, depth) {
  return text.replace(/^/gm, ' '.repeat(depth));
}

function fold(text, width) {
  return text.replace(new RegExp(`([^\n]{0,${width}})( |$)`, 'g'),
                      (_, newLine, end) => newLine + (end === ' ' ? '\n' : ''));
}

function getArgDescription(type) {
  switch (typeLookup[type]) {
    case 'kNoOp':
    case 'kV8Option':
    case 'kBoolean':
      break;
    case 'kHostPort':
      return '[host:]port';
    case 'kInteger':
    case 'kUInteger':
    case 'kString':
    case 'kStringList':
      return '...';
    case undefined:
      break;
    default:
      require('assert').fail(`unknown option type ${type}`);
  }
}

function format({ options, aliases = new Map(), firstColumn, secondColumn }) {
  let text = '';
  let maxFirstColumnUsed = 0;

  for (const [
    name, { helpText, type, value }
  ] of [...options.entries()].sort()) {
    if (!helpText) continue;

    let displayName = name;
    const argDescription = getArgDescription(type);
    if (argDescription)
      displayName += `=${argDescription}`;

    for (const [ from, to ] of aliases) {
      // For cases like e.g. `-e, --eval`.
      if (to[0] === name && to.length === 1) {
        displayName = `${from}, ${displayName}`;
      }

      // For cases like `--inspect-brk[=[host:]port]`.
      const targetInfo = options.get(to[0]);
      const targetArgDescription =
        targetInfo ? getArgDescription(targetInfo.type) : '...';
      if (from === `${name}=`) {
        displayName += `[=${targetArgDescription}]`;
      } else if (from === `${name} <arg>`) {
        displayName += ` [${targetArgDescription}]`;
      }
    }

    let displayHelpText = helpText;
    if (value === true) {
      // Mark boolean options we currently have enabled.
      // In particular, it indicates whether --use-openssl-ca
      // or --use-bundled-ca is the (current) default.
      displayHelpText += ' (currently set)';
    }

    text += displayName;
    maxFirstColumnUsed = Math.max(maxFirstColumnUsed, displayName.length);
    if (displayName.length >= firstColumn)
      text += '\n' + ' '.repeat(firstColumn);
    else
      text += ' '.repeat(firstColumn - displayName.length);

    text += indent(fold(displayHelpText, secondColumn),
                   firstColumn).trimLeft() + '\n';
  }

  if (maxFirstColumnUsed < firstColumn - 4) {
    // If we have more than 4 blank gap spaces, reduce first column width.
    return format({
      options,
      aliases,
      firstColumn: maxFirstColumnUsed + 2,
      secondColumn
    });
  }

  return text;
}

function print(stream) {
  const { options, aliases } = require('internal/options');

  // Use 75 % of the available width, and at least 70 characters.
  const width = Math.max(70, (stream.columns || 0) * 0.75);
  const firstColumn = Math.floor(width * 0.4);
  const secondColumn = Math.floor(width * 0.57);

  options.set('-', { helpText: 'script read from stdin ' +
                               '(default if no file name is provided, ' +
                               'interactive mode if a tty)' });
  options.set('--', { helpText: 'indicate the end of node options' });
  stream.write(
    'Usage: node [options] [ -e script | script.js | - ] [arguments]\n' +
    '       node inspect script.js [arguments]\n\n' +
    'Options:\n');
  stream.write(indent(format({
    options, aliases, firstColumn, secondColumn
  }), 2));

  stream.write('\nEnvironment variables:\n');

  stream.write(format({
    options: envVars, firstColumn, secondColumn
  }));

  stream.write('\nDocumentation can be found at https://nodejs.org/\n');
}

markBootstrapComplete();

print(process.stdout);
