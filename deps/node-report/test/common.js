'use strict';

const child_process = require('child_process');
const fs = require('fs');
const os = require('os');

const osMap = {
  'aix': 'AIX',
  'darwin': 'Darwin',
  'linux': 'Linux',
  'sunos': 'SunOS',
  'win32': 'Windows',
};

const REPORT_SECTIONS = [
  'Node Report',
  'JavaScript Stack Trace',
  'JavaScript Heap',
  'System Information'
];

const reNewline = '(?:\\r*\\n)';

exports.findReports = (pid) => {
  // Default filenames are of the form node-report.<date>.<time>.<pid>.<seq>.txt
  const format = '^node-report\\.\\d+\\.\\d+\\.' + pid + '\\.\\d+\\.txt$';
  const filePattern = new RegExp(format);
  const files = fs.readdirSync('.');
  return files.filter((file) => filePattern.test(file));
};

exports.isAIX = () => {
  return process.platform === 'aix';
};

exports.isPPC = () => {
  return process.arch.startsWith('ppc');
};

exports.isSunOS = () => {
  return process.platform === 'sunos';
};

exports.isWindows = () => {
  return process.platform === 'win32';
};

exports.validate = (t, report, options) => {
  t.test('Validating ' + report, (t) => {
    fs.readFile(report, (err, data) => { this.validateContent(data, t, options)})
  })
}

exports.validateContent = function validateContent(data, t, options) {
  const pid = options ? options.pid : process.pid;
  const reportContents = data.toString();
  const nodeComponents = Object.keys(process.versions);
  const expectedVersions = options ?
                           options.expectedVersions || nodeComponents :
                           nodeComponents;
  const expectedException = options.expectedException;
  if (options.expectedException) {
    REPORT_SECTIONS.push('JavaScript Exception Details');
  }

  let plan = REPORT_SECTIONS.length + nodeComponents.length + 5;
  if (options.commandline) plan++;
  if (options.expectedException) plan++;
  const glibcRE = /\(glibc:\s([\d.]+)/;
  const nodeReportSection = getSection(reportContents, 'Node Report');
  const sysInfoSection = getSection(reportContents, 'System Information');
  const exceptionSection = getSection(reportContents, 'JavaScript Exception Details');
  const libcPath = getLibcPath(sysInfoSection);
  const libcVersion = getLibcVersion(libcPath);
  if (glibcRE.test(nodeReportSection) && libcVersion) plan++;
  t.plan(plan);
  // Check all sections are present
  REPORT_SECTIONS.forEach((section) => {
    t.match(reportContents, new RegExp('==== ' + section),
            'Checking report contains ' + section + ' section');
  });

  // Check report header section
  t.match(nodeReportSection, new RegExp('Process ID: ' + pid),
          'Node Report header section contains expected process ID');
  if (options && options.expectNodeVersion === false) {
    t.match(nodeReportSection, /Unable to determine Node.js version/,
            'Node Report header section contains expected Node.js version');
  } else {
    t.match(nodeReportSection,
            new RegExp('Node.js version: ' + process.version),
            'Node Report header section contains expected Node.js version');
  }
  if (options && options.expectedException) {
	  t.match(exceptionSection, new RegExp('Uncaught Error: ' + options.expectedException),
      'Node Report JavaScript Exception contains expected message');
  }
  if (options && options.commandline) {
    if (this.isWindows()) {
      // On Windows we need to strip double quotes from the command line in
      // the report, and escape backslashes in the regex comparison string.
      t.match(nodeReportSection.replace(/"/g,''),
              new RegExp('Command line: '
                         + (options.commandline).replace(/\\/g,'\\\\')),
              'Checking report contains expected command line');
    } else {
      t.match(nodeReportSection,
              new RegExp('Command line: ' + options.commandline),
              'Checking report contains expected command line');
    }
  }
  nodeComponents.forEach((c) => {
    if (c !== 'node') {
      if (expectedVersions.indexOf(c) === -1) {
        t.notMatch(nodeReportSection,
                   new RegExp(c + ': ' + process.versions[c]),
                   'Node Report header section does not contain ' + c + ' version');
      } else {
        t.match(nodeReportSection,
                new RegExp(c + ': ' + process.versions[c]),
                'Node Report header section contains expected ' + c + ' version');
      }
    }
  });
  const nodereportMetadata = require('../package.json');
  t.match(nodeReportSection,
          new RegExp('node-report version: ' + nodereportMetadata.version),
          'Node Report header section contains expected Node Report version');
  const os = require('os');
  if (this.isWindows()) {
    t.match(nodeReportSection,
            new RegExp('Machine: ' + os.hostname(), 'i'), // ignore case on Windows
            'Checking machine name in report header section contains os.hostname()');
  } else if (this.isAIX()) {
    t.match(nodeReportSection,
            new RegExp('Machine: ' + os.hostname().split('.')[0]), // truncate on AIX
            'Checking machine name in report header section contains os.hostname()');
  } else {
    t.match(nodeReportSection,
            new RegExp('Machine: ' + os.hostname()),
            'Checking machine name in report header section contains os.hostname()');
  }

  const osName = osMap[os.platform()];
  const osVersion = nodeReportSection.match(/OS version: .*(?:\r*\n)/);
  if (this.isWindows()) {
    t.match(osVersion,
            new RegExp('OS version: ' + osName), 'Checking OS version');
  } else if (this.isAIX() && !os.release().includes('.')) {
    // For Node.js prior to os.release() fix for AIX:
    // https://github.com/nodejs/node/pull/10245
    t.match(osVersion,
            new RegExp('OS version: ' + osName + ' \\d+.' + os.release()),
            'Checking OS version');
  } else {
    t.match(osVersion,
            new RegExp('OS version: ' + osName + ' .*' + os.release()),
            'Checking OS version');
  }

  // Check report System Information section
  // If the report contains a glibc version, check it against libc.so.6
  const glibcMatch = glibcRE.exec(nodeReportSection);
  if (glibcMatch != null && libcVersion) {
    t.equal(glibcMatch[1], libcVersion,
            'Checking reported runtime glibc version against ' + libcPath);
  }
  // Find a line which ends with "/api.node" or "\api.node" (Unix or
  // Windows paths) to see if the library for node report was loaded.
  t.match(sysInfoSection, /  .*(\/|\\)api\.node/,
    'System Information section contains node-report library.');
};

const getLibcPath = (section) => {
  const libcMatch = /\n\s+(\/.*\/libc.so.6)\b/.exec(section);
  return (libcMatch != null ? libcMatch[1] : undefined);
};

const getLibcVersion = (path) => {
  if (!path) {
    return undefined;
  }
  const child = child_process.spawnSync('strings', [path], {encoding: 'utf8'});
  const match = /GNU C Library.*\bversion ([\d.]+)\b/.exec(child.stdout);
  return (match != null ? match[1] : undefined);
};

const getSection = exports.getSection = (report, section) => {
  const re = new RegExp('==== ' + section + ' =+' + reNewline + '+([\\S\\s]+?)'
                        + reNewline + '+={80}' + reNewline);
  const match = re.exec(report);
  return match ? match[1] : '';
};
