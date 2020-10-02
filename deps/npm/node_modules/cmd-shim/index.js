// On windows, create a .cmd file.
// Read the #! in the file to see what it uses.  The vast majority
// of the time, this will be either:
// "#!/usr/bin/env <prog> <args...>"
// or:
// "#!<prog> <args...>"
//
// Write a binroot/pkg.bin + ".cmd" file that has this line in it:
// @<prog> <args...> %dp0%<target> %*

const {promisify} = require('util')
const fs = require('fs')
const writeFile = promisify(fs.writeFile)
const readFile = promisify(fs.readFile)
const chmod = promisify(fs.chmod)
const stat = promisify(fs.stat)
const unlink = promisify(fs.unlink)

const {dirname, relative} = require('path')
const mkdir = require('mkdirp-infer-owner')
const toBatchSyntax = require('./lib/to-batch-syntax')
const shebangExpr = /^#\!\s*(?:\/usr\/bin\/env)?\s*([^ \t]+=[^ \t]+\s+)*\s*([^ \t]+)(.*)$/

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
  mkdir(dirname(to))
    .then(() => readFile(from, 'utf8'))
    .then(data => {
      const firstLine = data.trim().split(/\r*\n/)[0]
      const shebang = firstLine.match(shebangExpr)
      if (!shebang) return writeShim_(from, to)
      const vars = shebang[1] || ''
      const prog = shebang[2]
      const args = shebang[3] || ''
      return writeShim_(from, to, prog, args, vars)
    }, er => writeShim_(from, to))


const writeShim_ = (from, to, prog, args, variables) => {
  let shTarget = relative(dirname(to), from)
  let target = shTarget.split('/').join('\\')
  let longProg
  let shProg = prog && prog.split('\\').join('/')
  let shLongProg
  let pwshProg = shProg && `"${shProg}$exe"`
  let pwshLongProg
  shTarget = shTarget.split('\\').join('/')
  args = args || ''
  variables = variables || ''
  if (!prog) {
    prog = `"%dp0%\\${target}"`
    shProg = `"$basedir/${shTarget}"`
    pwshProg = shProg
    args = ''
    target = ''
    shTarget = ''
  } else {
    longProg = `"%dp0%\\${prog}.exe"`
    shLongProg = `"$basedir/${prog}"`
    pwshLongProg = `"$basedir/${prog}$exe"`
    target = `"%dp0%\\${target}"`
    shTarget = `"$basedir/${shTarget}"`
  }

  // @SETLOCAL
  // @CALL :find_dp0
  //
  // @IF EXIST "%dp0%\node.exe" (
  //   @SET "_prog=%dp0%\node.exe"
  // ) ELSE (
  //   @SET "_prog=node"
  //   @SET PATHEXT=%PATHEXT:;.JS;=;%
  // )
  //
  // "%_prog%" "%dp0%\.\node_modules\npm\bin\npm-cli.js" %*
  // @ENDLOCAL
  // @EXIT /b %errorlevel%
  //
  // :find_dp0
  // SET dp0=%~dp0
  // EXIT /b
  //
  // Subroutine trick to fix https://github.com/npm/cmd-shim/issues/10
  const head = '@ECHO off\r\n' +
    'SETLOCAL\r\n' +
    'CALL :find_dp0\r\n'
  const foot = 'ENDLOCAL\r\n' +
    'EXIT /b %errorlevel%\r\n' +
    ':find_dp0\r\n' +
    'SET dp0=%~dp0\r\n' +
    'EXIT /b\r\n'

  let cmd
  if (longProg) {
    shLongProg = shLongProg.trim();
    args = args.trim();
    const variablesBatch = toBatchSyntax.convertToSetCommands(variables)
    cmd = head
        + variablesBatch
        + '\r\n'
        + `IF EXIST ${longProg} (\r\n`
        + `  SET "_prog=${longProg.replace(/(^")|("$)/g, '')}"\r\n`
        + ') ELSE (\r\n'
        + `  SET "_prog=${prog.replace(/(^")|("$)/g, '')}"\r\n`
        + '  SET PATHEXT=%PATHEXT:;.JS;=;%\r\n'
        + ')\r\n'
        + '\r\n'
        +  `"%_prog%" ${args} ${target} %*\r\n`
        + foot
  } else {
    cmd = `${head}${prog} ${args} ${target} %*\r\n${foot}`
  }

  // #!/bin/sh
  // basedir=`dirname "$0"`
  //
  // case `uname` in
  //     *CYGWIN*|*MINGW*|*MSYS*) basedir=`cygpath -w "$basedir"`;;
  // esac
  //
  // if [ -x "$basedir/node.exe" ]; then
  //   "$basedir/node.exe" "$basedir/node_modules/npm/bin/npm-cli.js" "$@"
  //   ret=$?
  // else
  //   node "$basedir/node_modules/npm/bin/npm-cli.js" "$@"
  //   ret=$?
  // fi
  // exit $ret

  let sh = "#!/bin/sh\n"

  sh = sh
      + `basedir=$(dirname "$(echo "$0" | sed -e 's,\\\\,/,g')")\n`
      + '\n'
      + 'case `uname` in\n'
      + '    *CYGWIN*|*MINGW*|*MSYS*) basedir=`cygpath -w "$basedir"`;;\n'
      + 'esac\n'
      + '\n'

  if (shLongProg) {
    sh = sh
       + `if [ -x ${shLongProg} ]; then\n`
       + `  ${variables}${shLongProg} ${args} ${shTarget} "$@"\n`
       + '  ret=$?\n'
       + 'else \n'
       + `  ${variables}${shProg} ${args} ${shTarget} "$@"\n`
       + '  ret=$?\n'
       + 'fi\n'
       + 'exit $ret\n'
  } else {
    sh = sh
       + `${shProg} ${args} ${shTarget} "$@"\n`
       + 'exit $?\n'
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
  //   & "$basedir/node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
  //   $ret=$LASTEXITCODE
  // } else {
  //   & "node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
  //   $ret=$LASTEXITCODE
  // }
  // exit $ret
  let pwsh = '#!/usr/bin/env pwsh\n'
           + '$basedir=Split-Path $MyInvocation.MyCommand.Definition -Parent\n'
           + '\n'
           + '$exe=""\n'
           + 'if ($PSVersionTable.PSVersion -lt \"6.0\" -or $IsWindows) {\n'
           + '  # Fix case when both the Windows and Linux builds of Node\n'
           + '  # are installed in the same directory\n'
           + '  $exe=".exe"\n'
           + '}\n'
  if (shLongProg) {
    pwsh = pwsh
         + '$ret=0\n'
         + `if (Test-Path ${pwshLongProg}) {\n`
         + `  & ${pwshLongProg} ${args} ${shTarget} $args\n`
         + '  $ret=$LASTEXITCODE\n'
         + '} else {\n'
         + `  & ${pwshProg} ${args} ${shTarget} $args\n`
         + '  $ret=$LASTEXITCODE\n'
         + '}\n'
         + 'exit $ret\n'
  } else {
    pwsh = pwsh
         + `& ${pwshProg} ${args} ${shTarget} $args\n`
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
