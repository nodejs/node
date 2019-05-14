'use strict';
require('../common');
const assert = require('assert');
const child_process = require('child_process');

const p = child_process.spawnSync(
  process.execPath, [ '--completion-bash' ]);
assert.ifError(p.error);
assert.ok(p.stdout.toString().includes(
  `_node_complete() {
  local cur_word options
  cur_word="\${COMP_WORDS[COMP_CWORD]}"
  if [[ "\${cur_word}" == -* ]] ; then
    COMPREPLY=( $(compgen -W '`));
assert.ok(p.stdout.toString().includes(
  `' -- "\${cur_word}") )
    return 0
  else
    COMPREPLY=( $(compgen -f "\${cur_word}") )
    return 0
  fi
}
complete -F _node_complete node node_g`));
