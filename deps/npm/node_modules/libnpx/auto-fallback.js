'use strict'

const Y = require('./y.js')

function mkPosix (opts) {
  return `
command_not_found_${opts.isBash ? 'handle' : 'handler'}() {
  # Do not run within a pipe
  if test ! -t 1; then
    >&2 echo "${Y`command not found: ${'$1'}`}"
    return 127
  fi
  if which npx > /dev/null; then
    echo "${Y`${'$1'} not found. Trying with npx...`}" >&2
  else
    return 127
  fi
  if ! [[ $1 =~ @ ]]; then
    npx --no-install "$@"
  else
    npx "$@"
  fi
  return $?
}`
}

function mkFish (opts) {
  return `
function __fish_command_not_found_on_interactive --on-event fish_prompt
  functions --erase __fish_command_not_found_handler
  functions --erase __fish_command_not_found_setup

  function __fish_command_not_found_handler --on-event fish_command_not_found
    if which npx > /dev/null
        echo "${Y`${'$argv[1]'} not found. Trying with npx...`}" >&2
    else
        return 127
    end
    if string match -q -r @ $argv[1]
        npx $argv
    else
        npx --no-install $argv
    end
  end

  functions --erase __fish_command_not_found_on_interactive
end`
}

module.exports = autoFallback
function autoFallback (shell, fromEnv, opts) {
  if (shell.includes('bash')) {
    return mkPosix({isBash: true, install: opts.install})
  }

  if (shell.includes('zsh')) {
    return mkPosix({isBash: false, install: opts.install})
  }

  if (shell.includes('fish')) {
    return mkFish(opts)
  }

  if (fromEnv) {
    return autoFallback(fromEnv, null, opts)
  }

  console.error(Y`Only Bash, Zsh, and Fish shells are supported :(`)
}
