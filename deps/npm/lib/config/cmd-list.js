// short names for common things
var shorthands = {
  'un': 'uninstall',
  'rb': 'rebuild',
  'list': 'ls',
  'ln': 'link',
  'i': 'install',
  'it': 'install-test',
  'up': 'update',
  'c': 'config',
  's': 'search',
  'se': 'search',
  'unstar': 'star', // same function
  'tst': 'test',
  't': 'test',
  'ddp': 'dedupe',
  'v': 'view',
  'run': 'run-script'
}

var affordances = {
  'la': 'ls',
  'll': 'ls',
  'verison': 'version',
  'ic': 'ci',
  'isntall': 'install',
  'dist-tags': 'dist-tag',
  'apihelp': 'help',
  'find-dupes': 'dedupe',
  'upgrade': 'update',
  'udpate': 'update',
  'login': 'adduser',
  'add-user': 'adduser',
  'author': 'owner',
  'home': 'docs',
  'issues': 'bugs',
  'info': 'view',
  'show': 'view',
  'find': 'search',
  'add': 'install',
  'unlink': 'uninstall',
  'remove': 'uninstall',
  'rm': 'uninstall',
  'r': 'uninstall',
  'rum': 'run-script'
}

// these are filenames in .
var cmdList = [
  'ci',
  'install',
  'install-test',
  'uninstall',
  'cache',
  'config',
  'set',
  'get',
  'update',
  'outdated',
  'prune',
  'pack',
  'dedupe',

  'rebuild',
  'link',

  'publish',
  'star',
  'stars',
  'adduser',
  'login', // This is an alias for `adduser` but it can be confusing
  'logout',
  'unpublish',
  'owner',
  'access',
  'team',
  'deprecate',
  'shrinkwrap',
  'token',
  'profile',

  'help',
  'help-search',
  'ls',
  'search',
  'view',
  'init',
  'version',
  'edit',
  'explore',
  'docs',
  'repo',
  'bugs',
  'root',
  'prefix',
  'bin',
  'whoami',
  'dist-tag',
  'ping',

  'test',
  'stop',
  'start',
  'restart',
  'run-script',
  'completion',
  'doctor'
]

var plumbing = [
  'build',
  'unbuild',
  'xmas',
  'substack',
  'visnup'
]
module.exports.aliases = Object.assign({}, shorthands, affordances)
module.exports.shorthands = shorthands
module.exports.affordances = affordances
module.exports.cmdList = cmdList
module.exports.plumbing = plumbing
