/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/utils/error-message.js TAP 404 name with error > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "404",
      "",
    ],
    Array [
      "404",
      "",
      "'node_modules' is not in the npm registry.",
    ],
    Array [
      "404",
      "This package name is not valid, because",
      "",
    ],
    Array [
      "404",
      " 1. node_modules is a blacklisted name",
    ],
    Array [
      "404",
      "\\nNote that you can also install from a",
    ],
    Array [
      "404",
      "tarball, folder, http url, or git url.",
    ],
  ],
  "summary": Array [
    Array [
      "404",
      "not found",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP 404 name with warning > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "404",
      "",
    ],
    Array [
      "404",
      "",
      "'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' is not in the npm registry.",
    ],
    Array [
      "404",
      "This package name is not valid, because",
      "",
    ],
    Array [
      "404",
      " 1. name can no longer contain more than 214 characters",
    ],
    Array [
      "404",
      "\\nNote that you can also install from a",
    ],
    Array [
      "404",
      "tarball, folder, http url, or git url.",
    ],
  ],
  "summary": Array [
    Array [
      "404",
      "not found",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP 404 no package id > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "404",
      "not found",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP 404 you should publish it > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "404",
      "",
    ],
    Array [
      "404",
      "",
      "'yolo' is not in the npm registry.",
    ],
    Array [
      "404",
      "You should bug the author to publish it (or use the name yourself!)",
    ],
    Array [
      "404",
      "\\nNote that you can also install from a",
    ],
    Array [
      "404",
      "tarball, folder, http url, or git url.",
    ],
  ],
  "summary": Array [
    Array [
      "404",
      "not found",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP bad engine with config loaded > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "notsup",
      "Not compatible with your version of node/npm: some@package\\nRequired: undefined\\nActual:   {\\"npm\\":\\"123.69.420-npm\\",\\"node\\":\\"99.99.99\\"}",
    ],
  ],
  "summary": Array [
    Array [
      "engine",
      "foo",
    ],
    Array [
      "engine",
      "Not compatible with your version of node/npm: some@package",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP bad platform array os/arch > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "notsup",
      "Valid OS:    !yours,mine\\nValid Arch:  x420,x69\\nActual OS:   posix\\nActual Arch: x64",
    ],
  ],
  "summary": Array [
    Array [
      "notsup",
      "Unsupported platform for lodash@1.0.0: wanted {\\"os\\":\\"!yours,mine\\",\\"arch\\":\\"x420,x69\\"} (current: {\\"os\\":\\"posix\\",\\"arch\\":\\"x64\\"})",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP bad platform string os/arch > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "notsup",
      "Valid OS:    !yours\\nValid Arch:  x420\\nActual OS:   posix\\nActual Arch: x64",
    ],
  ],
  "summary": Array [
    Array [
      "notsup",
      "Unsupported platform for lodash@1.0.0: wanted {\\"os\\":\\"!yours\\",\\"arch\\":\\"x420\\"} (current: {\\"os\\":\\"posix\\",\\"arch\\":\\"x64\\"})",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP default message > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "error object",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP default message > must match snapshot 2`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "error string",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP default message > must match snapshot 3`] = `
Object {
  "detail": Array [
    Array [
      "signal",
      "SIGYOLO",
    ],
    Array [
      "command",
      "some command",
      "a",
      "r",
      "g",
      "s",
    ],
    Array [
      "",
      "stdout",
    ],
    Array [
      "",
      "stderr",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "cmd err",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":false,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt is likely you do not have the permissions to access this file as the current user\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/not/cache/dir/dest",
        "path": "/not/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":false,"cacheDest":false} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":false,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt is likely you do not have the permissions to access this file as the current user\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/some/cache/dir/dest",
        "path": "/not/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":false,"cacheDest":true} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":true,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt is likely you do not have the permissions to access this file as the current user\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/not/cache/dir/dest",
        "path": "/some/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":true,"cacheDest":false} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":true,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt is likely you do not have the permissions to access this file as the current user\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/some/cache/dir/dest",
        "path": "/some/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":false,"cachePath":true,"cacheDest":true} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":false,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt is likely you do not have the permissions to access this file as the current user\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/not/cache/dir/dest",
        "path": "/not/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":false,"cacheDest":false} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":false,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "\\nYour cache folder contains root-owned files, due to a bug in\\nprevious versions of npm which has since been addressed.\\n\\nTo permanently fix this problem, please run:\\n  sudo chown -R 69:420 \\"/some/cache/dir\\"",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":false,"cacheDest":true} > must match snapshot 2`] = `
Array [
  Array [
    "dummy stack trace",
  ],
]
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":true,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "\\nYour cache folder contains root-owned files, due to a bug in\\nprevious versions of npm which has since been addressed.\\n\\nTo permanently fix this problem, please run:\\n  sudo chown -R 69:420 \\"/some/cache/dir\\"",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":true,"cacheDest":false} > must match snapshot 2`] = `
Array [
  Array [
    "dummy stack trace",
  ],
]
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":true,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "\\nYour cache folder contains root-owned files, due to a bug in\\nprevious versions of npm which has since been addressed.\\n\\nTo permanently fix this problem, please run:\\n  sudo chown -R 69:420 \\"/some/cache/dir\\"",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":false,"loaded":true,"cachePath":true,"cacheDest":true} > must match snapshot 2`] = `
Array [
  Array [
    "dummy stack trace",
  ],
]
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":false,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/not/cache/dir/dest",
        "path": "/not/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":false,"cacheDest":false} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":false,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/some/cache/dir/dest",
        "path": "/not/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":false,"cacheDest":true} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":true,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/not/cache/dir/dest",
        "path": "/some/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":true,"cacheDest":false} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":true,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/some/cache/dir/dest",
        "path": "/some/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":false,"cachePath":true,"cacheDest":true} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":false,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/not/cache/dir/dest",
        "path": "/not/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":false,"cacheDest":false} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":false,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/some/cache/dir/dest",
        "path": "/not/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":false,"cacheDest":true} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":true,"cacheDest":false} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/not/cache/dir/dest",
        "path": "/some/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":true,"cacheDest":false} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":true,"cacheDest":true} > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nThe operation was rejected by your operating system.\\nIt's possible that the file was already in use (by a text editor or antivirus),\\nor that you lack permissions to access it.\\n\\nIf you believe this might be a permissions issue, please double-check the\\npermissions of the file and its containing directories, or try running\\nthe command again as root/Administrator.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: whoopsie {
        "code": "EACCES",
        "dest": "/some/cache/dir/dest",
        "path": "/some/cache/dir/path",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eacces/eperm {"windows":true,"loaded":true,"cachePath":true,"cacheDest":true} > must match snapshot 2`] = `
Array []
`

exports[`test/lib/utils/error-message.js TAP enoent without a file > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "enoent",
      "This is related to npm not being able to find a file.\\n",
    ],
  ],
  "summary": Array [
    Array [
      "enoent",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP enolock without a command > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "Try creating one first with: npm i --package-lock-only",
    ],
    Array [
      "",
      "Original error: foo",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "This command requires an existing lockfile.",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 401, no auth headers > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "nope",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 401, no message > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      Object {
        "code": "E401",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 one-time pass challenge code > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "You can provide a one-time password by passing --otp=<code> to the command you ran.\\nIf you already provided a one-time password then it is likely that you either typoed\\nit, or it timed out. Please try again.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "This operation requires a one-time password from your authenticator.",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 one-time pass challenge message > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "You can provide a one-time password by passing --otp=<code> to the command you ran.\\nIf you already provided a one-time password then it is likely that you either typoed\\nit, or it timed out. Please try again.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "This operation requires a one-time password from your authenticator.",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 www-authenticate challenges Basic realm=by, charset="UTF-8", challenge="your friends" > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "If you were trying to login, change your password, create an\\nauthentication token or enable two-factor authentication then\\nthat means you likely typed your password in incorrectly.\\nPlease try again, or recover your password at:\\n    https://www.npmjs.com/forgot\\n\\nIf you were doing some other operation then your saved credentials are\\nprobably out of date. To correct this please try logging in again with:\\n    npm login",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "Incorrect or missing password.",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 www-authenticate challenges Bearer realm=do, charset="UTF-8", challenge="yourself" > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "To correct this please trying logging in again with:\\n    npm login",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "Unable to authenticate, your authentication token seems to be invalid.",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 www-authenticate challenges PickACardAnyCard realm=friday, charset="UTF-8" > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "challenge!",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP eotp/e401 www-authenticate challenges WashYourHands, charset="UTF-8" > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "",
      "challenge!",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP explain ERESOLVE errors > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "",
    ],
    Array [
      "",
      "explanation",
    ],
  ],
  "summary": Array [
    Array [
      "ERESOLVE",
      "could not resolve",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "audit",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 10`] = `
Object {
  "detail": Array [
    Array [
      "network",
      "This is a problem related to network connectivity.\\nIn most cases you are behind a proxy or have bad network settings.\\n\\nIf you are behind a proxy, please make sure that the\\n'proxy' config is set properly.  See: 'npm help config'",
    ],
  ],
  "summary": Array [
    Array [
      "network",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 11`] = `
Object {
  "detail": Array [
    Array [
      "network",
      "This is a problem related to network connectivity.\\nIn most cases you are behind a proxy or have bad network settings.\\n\\nIf you are behind a proxy, please make sure that the\\n'proxy' config is set properly.  See: 'npm help config'",
    ],
  ],
  "summary": Array [
    Array [
      "network",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 12`] = `
Object {
  "detail": Array [
    Array [
      "network",
      "This is a problem related to network connectivity.\\nIn most cases you are behind a proxy or have bad network settings.\\n\\nIf you are behind a proxy, please make sure that the\\n'proxy' config is set properly.  See: 'npm help config'",
    ],
  ],
  "summary": Array [
    Array [
      "network",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 13`] = `
Object {
  "detail": Array [
    Array [
      "notsup",
      "Not compatible with your version of node/npm: some@package\\nRequired: undefined\\nActual:   {\\"npm\\":\\"123.69.420-npm\\",\\"node\\":\\"123.69.420-node\\"}",
    ],
  ],
  "summary": Array [
    Array [
      "engine",
      "foo",
    ],
    Array [
      "engine",
      "Not compatible with your version of node/npm: some@package",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 14`] = `
Object {
  "detail": Array [
    Array [
      "nospc",
      "There appears to be insufficient space on your system to finish.\\nClear up some disk space and try again.",
    ],
  ],
  "summary": Array [
    Array [
      "nospc",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 15`] = `
Object {
  "detail": Array [
    Array [
      "rofs",
      "Often virtualized file systems, or other file systems\\nthat don't support symlinks, give this error.",
    ],
  ],
  "summary": Array [
    Array [
      "rofs",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 16`] = `
Object {
  "detail": Array [
    Array [
      "enoent",
      "This is related to npm not being able to find a file.\\n\\nCheck if the file '/some/file' is present.",
    ],
  ],
  "summary": Array [
    Array [
      "enoent",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 17`] = `
Object {
  "detail": Array [
    Array [
      "typeerror",
      "This is an error with npm itself. Please report this error at:\\n    https://github.com/npm/cli/issues",
    ],
  ],
  "summary": Array [
    Array [
      "typeerror",
      "dummy stack trace",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 18`] = `
Object {
  "detail": Array [
    Array [
      "typeerror",
      "This is an error with npm itself. Please report this error at:\\n    https://github.com/npm/cli/issues",
    ],
  ],
  "summary": Array [
    Array [
      "typeerror",
      "dummy stack trace",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 19`] = `
Object {
  "detail": Array [
    Array [
      "typeerror",
      "This is an error with npm itself. Please report this error at:\\n    https://github.com/npm/cli/issues",
    ],
  ],
  "summary": Array [
    Array [
      "typeerror",
      "dummy stack trace",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 2`] = `
Object {
  "detail": Array [
    Array [
      "audit",
      "Try creating one first with: npm i --package-lock-only",
    ],
    Array [
      "audit",
      "Original error: foo",
    ],
  ],
  "summary": Array [
    Array [
      "audit",
      "This command requires an existing lockfile.",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 20`] = `
Object {
  "detail": Array [
    Array [
      "typeerror",
      "This is an error with npm itself. Please report this error at:\\n    https://github.com/npm/cli/issues",
    ],
  ],
  "summary": Array [
    Array [
      "typeerror",
      "dummy stack trace",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 21`] = `
Object {
  "detail": Array [
    Array [
      "notarget",
      "In most cases you or one of your dependencies are requesting\\na package version that doesn't exist.",
    ],
  ],
  "summary": Array [
    Array [
      "notarget",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 22`] = `
Object {
  "detail": Array [
    Array [
      "403",
      "In most cases, you or one of your dependencies are requesting\\na package version that is forbidden by your security policy, or\\non a server you do not have access to.",
    ],
  ],
  "summary": Array [
    Array [
      "403",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 3`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nIf you are behind a proxy, please make sure that the\\n'proxy' config is set properly.  See: 'npm help config'",
    ],
  ],
  "summary": Array [
    Array [
      "",
      Error: foo {
        "code": "ECONNREFUSED",
        "file": "/some/file",
        "path": "/some/path",
        "pkgid": "some@package",
      },
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 4`] = `
Object {
  "detail": Array [
    Array [
      "",
      "\\nFailed using git.\\nPlease check if you have git installed and in your PATH.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 5`] = `
Object {
  "detail": Array [
    Array [
      "publish fail",
      "Update the 'version' field in package.json and try again.",
    ],
    Array [
      "publish fail",
      "",
    ],
    Array [
      "publish fail",
      "To automatically increment version numbers, see:",
    ],
    Array [
      "publish fail",
      "    npm help version",
    ],
  ],
  "summary": Array [
    Array [
      "publish fail",
      "Cannot publish over existing version.",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 6`] = `
Object {
  "detail": Array [
    Array [
      "git",
      "Refusing to remove it. Update manually,\\nor move it out of the way first.",
    ],
  ],
  "summary": Array [
    Array [
      "git",
      "foo",
    ],
    Array [
      "git",
      "    /some/path",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 7`] = `
Object {
  "detail": Array [
    Array [
      "",
      "Remove the existing file and try again, or run npm",
    ],
    Array [
      "",
      "with --force to overwrite files recklessly.",
    ],
  ],
  "summary": Array [
    Array [
      "",
      "foo",
    ],
    Array [
      "",
      "File exists: /some/path",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 8`] = `
Object {
  "detail": Array [
    Array [
      "need auth",
      "You need to authorize this machine using \`npm adduser\`",
    ],
  ],
  "summary": Array [
    Array [
      "need auth",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP just simple messages > must match snapshot 9`] = `
Object {
  "detail": Array [
    Array [
      "network",
      "This is a problem related to network connectivity.\\nIn most cases you are behind a proxy or have bad network settings.\\n\\nIf you are behind a proxy, please make sure that the\\n'proxy' config is set properly.  See: 'npm help config'",
    ],
  ],
  "summary": Array [
    Array [
      "network",
      "foo",
    ],
  ],
}
`

exports[`test/lib/utils/error-message.js TAP replace message/stack sensistive info > must match snapshot 1`] = `
Object {
  "detail": Array [],
  "summary": Array [
    Array [
      "audit",
      "Error at registry: https://user:***@registry.npmjs.org/",
    ],
  ],
}
`

exports[`v TAP json parse json somewhere else > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "JSON.parse",
      "Failed to parse JSON data.\\nNote: package.json must be actual JSON, not just JavaScript.",
    ],
  ],
  "summary": Array [
    Array [
      "JSON.parse",
      "not json",
    ],
  ],
}
`

exports[`v TAP json parse just regular bad json in package.json > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "JSON.parse",
      "Failed to parse JSON data.\\nNote: package.json must be actual JSON, not just JavaScript.",
    ],
  ],
  "summary": Array [
    Array [
      "JSON.parse",
      "not json",
    ],
  ],
}
`

exports[`v TAP json parse merge conflict in package.json > must match snapshot 1`] = `
Object {
  "detail": Array [
    Array [
      "",
      "Merge conflict detected in your package.json.\\n\\nPlease resolve the package.json conflict and retry the command:\\n\\n$ arg v",
    ],
  ],
  "summary": Array [],
}
`
