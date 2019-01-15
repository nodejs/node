'use strict';
const { options, aliases } = require('internal/options');

function print(stream) {
  const all_opts = [...options.keys(), ...aliases.keys()];

  stream.write(`_node_complete() {
  local cur_word options
  cur_word="\${COMP_WORDS[COMP_CWORD]}"
  if [[ "\${cur_word}" == -* ]] ; then
    COMPREPLY=( $(compgen -W '${all_opts.join(' ')}' -- "\${cur_word}") )
    return 0
  else
    COMPREPLY=( $(compgen -f "\${cur_word}") )
    return 0
  fi
}
complete -F _node_complete node node_g`);
}

markBootstrapComplete();

print(process.stdout);
