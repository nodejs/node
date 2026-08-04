// On windows, create a .cmd file.
// Read the #! in the file to see what it uses.  The vast majority
// of the time, this will be either:
// "#!/usr/bin/env <prog> <args...>"
// or:
// "#!<prog> <args...>"
//
// Write a binroot/pkg.bin + ".cmd" file that has this line in it:
// @<prog> <args...> %dp0%<target> %*

const {
  chmod,
  mkdir,
  readFile,
  stat,
  unlink,
  writeFile,
} = require('fs/promises')

const { dirname, relative } = require('path')
const toBatchSyntax = require('./to-batch-syntax')
// linting disabled because this regex is really long
// eslint-disable-next-line max-len
const shebangExpr = /^#!\s*(?:\/usr\/bin\/env\s+(?:-S\s+)?((?:[^ \t=]+=[^ \t=]+\s+)*))?([^ \t]+)(.*)$/

const cmdShimIfExists = (from, to) =>
  stat(from).then(() => cmdShim(from, to), () => {})

// Try to unlink, but ignore errors.
// Any problems will surface later.
const rm = path => unlink(path).catch(() => {})

const cmdShim = (from, to) =>
  stat(from).then(() => cmdShim_(from, to))

const cmdShim_ = (from, to) => Promise.all([
  rm(to),
  rm(to + '.cmd'),
  rm(to + '.ps1'),
]).then(() => writeShim(from, to))

const writeShim = (from, to) =>
  // make a cmd file and a sh script
  // First, check if the bin is a #! of some sort.
  // If not, then assume it's something that'll be compiled, or some other
  // sort of script, and just call it directly.
  mkdir(dirname(to), { recursive: true })
    .then(() => readFile(from, 'utf8'))
    .then(data => {
      const firstLine = data.trim().split(/\r*\n/)[0]
      const shebang = firstLine.match(shebangExpr)
      if (!shebang) {
        return writeShim_(from, to)
      }
      const vars = shebang[1] || ''
      const prog = shebang[2]
      const args = shebang[3] || ''
      return writeShim_(from, to, prog, args, vars)
    }, () => writeShim_(from, to))

const writeShim_ = (from, to, prog, args, variables) => {
  let shTarget = relative(dirname(to), from)
  shTarget = shTarget.split('\\').join('/')
  let target = shTarget.split('/').join('\\')
  let pwshTarget = shTarget
  let longProg
  let shProg = prog && prog.split('\\').join('/')
  let shLongProg
  let pwshProg = shProg && `"${shProg}$exe"`
  let pwshLongProg
  args = args || ''
  variables = variables || ''
  if (!prog) {
    prog = `"%dp0%\\${target}"`
    shProg = `"$basedir/${shTarget}"`
    pwshProg = shProg
    args = ''
    target = ''
    shTarget = ''
    pwshTarget = ''
  } else {
    longProg = `"%dp0%\\${prog}.exe"`
    shLongProg = `"$basedir/${prog}"`
    pwshLongProg = `"$basedir/${prog}$exe"`
    target = `"%dp0%\\${target}"`
    shTarget = `"$basedir_win/${shTarget}"`
    pwshTarget = `"$basedir/${pwshTarget}"`
  }

  // Subroutine trick to fix https://github.com/npm/cmd-shim/issues/10
  // and https://github.com/npm/cli/issues/969
  const head = '@ECHO off\r\n' +
    'GOTO start\r\n' +
    ':find_dp0\r\n' +
    'SET dp0=%~dp0\r\n' +
    'EXIT /b\r\n' +
    ':start\r\n' +
    'SETLOCAL\r\n' +
    'CALL :find_dp0\r\n'

  let cmd
  if (longProg) {
    args = args.trim()
    const variablesBatch = toBatchSyntax.convertToSetCommands(variables)
    cmd = head
        + variablesBatch
        + '\r\n'
        + `IF EXIST ${longProg} (\r\n`
        + `  SET "_prog=${longProg.replace(/(^")|("$)/g, '')}"\r\n`
        + ') ELSE (\r\n'
        + `  SET "_prog=${prog.replace(/(^")|("$)/g, '')}"\r\n`
        + ')\r\n'
        + '\r\n'
        // prevent "Terminate Batch Job? (Y/n)" message
        // https://github.com/npm/cli/issues/969#issuecomment-737496588
        + 'endLocal & goto #_undefined_# 2>NUL || title %COMSPEC% & '
        + `set PATHEXT=%PATHEXT:;.JS;=;% & "%_prog%" ${args} ${target} %*\r\n`
  } else {
    cmd = `${head}${prog} ${args} ${target} %*\r\n`
  }

  // #!/bin/sh
  // basedir=$(dirname "$(echo "$0" | sed -e 's,\\,/,g')")
  // basedir_win="$basedir"
  //
  // case `uname -a` in
  //   *CYGWIN*|*MINGW*|*MSYS*)
  //     if command -v cygpath > /dev/null 2>&1; then
  //       basedir_win=`cygpath -w "$basedir"`
  //     fi
  //   ;;
  //   *WSL2*)
  //     if command -v wslpath > /dev/null 2>&1; then
  //       basedir_win="$(wslpath -w "$basedir" 2> /dev/null)"
  //       if [ $? -ne 0 ] || [ -z "$basedir_win" ]; then
  //         echo "Error: wslpath failed to convert path. WSL environment may be misconfigured." >&2
  //         exit 1
  //       fi
  //     fi
  //   ;;
  // esac
  //
  // PROG_EXE="$basedir/node.exe"
  // if ! [ -x "$PROG_EXE" ]; then
  //   PROG_EXE="$basedir/node"
  //   if ! [ -x "$PROG_EXE" ]; then
  //     PROG_EXE=node
  //     if ! [ -x "$PROG_EXE" ]; then
  //       PROG_EXE=node.exe
  //     fi
  //   fi
  // fi
  //
  // exec "$PROG_EXE"  "$basedir_win/node_modules/npm/bin/npm-cli.js" "$@"

  let sh = '#!/bin/sh\n'

  sh = sh
      + `basedir=$(dirname "$(echo "$0" | sed -e 's,\\\\,/,g')")\n`
      + 'basedir_win="$basedir"\n'
      + '\n'
      + 'case `uname -a` in\n'
      + '  *CYGWIN*|*MINGW*|*MSYS*)\n'
      + '    if command -v cygpath > /dev/null 2>&1; then\n'
      + '      basedir_win=`cygpath -w "$basedir"`\n'
      + '    fi\n'
      + '  ;;\n'
      + '  *WSL2*)\n'
      + '    if command -v wslpath > /dev/null 2>&1; then\n'
      + '      basedir_win="$(wslpath -w "$basedir" 2> /dev/null)"\n'
      + '      if [ $? -ne 0 ] || [ -z "$basedir_win" ]; then\n'
      + '        echo "Error: wslpath failed to convert path. WSL environment may be misconfigured." >&2\n'
      + '        exit 1\n'
      + '      fi\n'
      + '    fi\n'
      + '  ;;\n'
      + 'esac\n'
      + '\n'

  if (shLongProg) {
    sh = sh
       + `PROG_EXE=${shLongProg.replace(/"$/, '.exe"')}\n`
       + 'if ! [ -x "$PROG_EXE" ]; then\n'
       + `  PROG_EXE=${shLongProg}\n`
       + '  if ! [ -x "$PROG_EXE" ]; then\n'
       + `    PROG_EXE=${shProg}\n`
       + '    if ! [ -x "$PROG_EXE" ]; then\n'
       + `      PROG_EXE=${shProg}.exe\n`
       + '    fi\n'
       + '  fi\n'
       + 'fi\n'
       + '\n'
       + `exec ${variables}"$PROG_EXE" ${args} ${shTarget} "$@"\n`
  } else {
    sh = sh
       + `exec ${shProg} ${args} ${shTarget} "$@"\n`
  }

  // #!/usr/bin/env pwsh
  // $basedir=Split-Path $MyInvocation.MyCommand.Definition -Parent
  //
  // $ret=0
  // $exe = ""
  // if ($PSVersionTable.PSVersion -lt "6.0" -or $IsWindows) {
  //   # Fix case when both the Windows and Linux builds of Node
  //   # are installed in the same directory
  //   $exe = ".exe"
  // }
  // if (Test-Path "$basedir/node") {
  //   # Suport pipeline input
  //   if ($MyInvocation.ExpectingInput) {
  //     input | & "$basedir/node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
  //   } else {
  //     & "$basedir/node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
  //   }
  //   $ret=$LASTEXITCODE
  // } else {
  //   # Support pipeline input
  //   if ($MyInvocation.ExpectingInput) {
  //     $input | & "node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
  //   } else {
  //     & "node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
  //   }
  //   $ret=$LASTEXITCODE
  // }
  // exit $ret
  let pwsh = '#!/usr/bin/env pwsh\n'
           + '$basedir=Split-Path $MyInvocation.MyCommand.Definition -Parent\n'
           + '\n'
           + '$exe=""\n'
           + 'if ($PSVersionTable.PSVersion -lt "6.0" -or $IsWindows) {\n'
           + '  # Fix case when both the Windows and Linux builds of Node\n'
           + '  # are installed in the same directory\n'
           + '  $exe=".exe"\n'
           + '}\n'
  if (pwshLongProg) {
    pwsh = pwsh
         + '$ret=0\n'
         + `if (Test-Path ${pwshLongProg}) {\n`
         + '  # Support pipeline input\n'
         + '  if ($MyInvocation.ExpectingInput) {\n'
         + `    $input | & ${pwshLongProg} ${args} ${pwshTarget} $args\n`
         + '  } else {\n'
         + `    & ${pwshLongProg} ${args} ${pwshTarget} $args\n`
         + '  }\n'
         + '  $ret=$LASTEXITCODE\n'
         + '} else {\n'
         + '  # Support pipeline input\n'
         + '  if ($MyInvocation.ExpectingInput) {\n'
         + `    $input | & ${pwshProg} ${args} ${pwshTarget} $args\n`
         + '  } else {\n'
         + `    & ${pwshProg} ${args} ${pwshTarget} $args\n`
         + '  }\n'
         + '  $ret=$LASTEXITCODE\n'
         + '}\n'
         + 'exit $ret\n'
  } else {
    pwsh = pwsh
         + '# Support pipeline input\n'
         + 'if ($MyInvocation.ExpectingInput) {\n'
         + `  $input | & ${pwshProg} ${args} ${pwshTarget} $args\n`
         + '} else {\n'
         + `  & ${pwshProg} ${args} ${pwshTarget} $args\n`
         + '}\n'
         + 'exit $LASTEXITCODE\n'
  }

  return Promise.all([
    writeFile(to + '.ps1', pwsh, 'utf8'),
    writeFile(to + '.cmd', cmd, 'utf8'),
    writeFile(to, sh, 'utf8'),
  ]).then(() => chmodShim(to))
}

const chmodShim = to => Promise.all([
  chmod(to, 0o755),
  chmod(to + '.cmd', 0o755),
  chmod(to + '.ps1', 0o755),
])

module.exports = cmdShim
cmdShim.ifExists = cmdShimIfExists
