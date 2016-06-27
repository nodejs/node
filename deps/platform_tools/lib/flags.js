'use strict';
let linkerArch = ''
switch (process.arch) {
  case 'x64': linkerArch = 'X64'; break;
  case 'ia32': linkerArch = 'X84'; break;
  case 'arm': linkerArch = 'ARM'; break;
}

exports.linux = {

}

exports.darwin = {

}

exports.windows = {
  vsStdIncludePaths: [
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\10\\Include\\10.0.10240.0\\ucrt`, //REVIEW
    `${process.env['ProgramFiles(x86)']}\\MSBuild\\Microsoft.Cpp\\v4.0V140`,
    `${process.env['ProgramFiles(x86)']}\\MSBuild\\Microsoft.Cpp\\v4.0`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\bin\\x86`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\bin\\x64`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\bin\\arm`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\lib\\arm`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\lib\\winv6.3\\um\\x86`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\lib\\winv6.3\\um\\x64`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\lib\\winv6.3\\um\\arm`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\um`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\shared`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\winrt`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\VC\\atlmfc\\include`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\VC\\include`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\VC\\bin`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\lib`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\lib\\amd64`,
    `${process.env['ProgramFiles(x86)']}\\MSBuild\\Microsoft.Cpp\\v4.0\V140`,
    `${process.env['ProgramFiles(x86)']}\\MSBuild\\Microsoft.Cpp\\v4.0`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\um`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\shared`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\winrt`
  ],
  vsStdLibPaths: [
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\bin\\x86`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 11.0\\VC\\bin`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\10\\Lib\\10.0.10240.0\\ucrt\\x64`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\VC\\lib\\amd64`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Lib\\winv6.3\\um\\x64`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\10\\Include\\10.0.10240.0\\ucr`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\VC\\atlmfc\\include`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\VC\\include`,
    `${process.env['ProgramFiles(x86)']}\\Microsoft Visual Studio 14.0\\VC\\include`,
    `${process.env['ProgramFiles(x86)']}\\MSBuild\\Microsoft.Cpp\\v4.0V140`,
    `${process.env['ProgramFiles(x86)']}\\MSBuild\\Microsoft.Cpp\\v4.0`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\um`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\shared`,
    `${process.env['ProgramFiles(x86)']}\\Windows Kits\\8.1\\Include\\winrt`,
  ]
}

exports.addon = {
  darwin: {
    compiler_flags: [
      '-Os',
      /*'-gdwarf-2' */,
      '-mmacosx-version-min=10.7',
      '-Wall',
      '-Wall',
      '-Wendif-labels',
      '-W',
      '-Wno-unused-parameter',
      '-std=gnu++0x',
      '-fno-rtti',
      '-fno-exceptions',
      '-fno-threadsafe-statics',
      '-fno-strict-aliasing'
    ],
    linker_flags: [
      '-bundle',
      '-undefined',
      'dynamic_lookup',
      '-Wl,-no_pie',
      '-Wl,-search_paths_first',
      '-mmacosx-version-min=10.7'
    ]
  },
  linux: {
    compiler_flags: [
      '-fPIC',
      '-pthread',
      '-Wall',
      '-Wextra',
      '-Wno-unused-parameter',
      '-m64',
      '-O3','-ffunction-sections',
      '-fdata-sections',
      '-fno-omit-frame-pointer',
      '-fno-rtti',
      '-fno-exceptions',
      // NOTE: -std=c++0x is for gcc earlier than 4.7. In the future this might
      // need to be checked, since 4.7 and 4.8 are more recent ones and may
      // be used more widely
      '-std=c++11'
    ],
    linker_flags: [
      '-shared',
      '-pthread',
      '-rdynamic',
      '-m64',
      '-Wl,--start-group',
      /*${process.cwd()}/build/${options.output}.o`,*/
      '-Wl,--end-group'
    ]
  },
  windows: {
    compiler_flags: [
      '/W3',
      '/WX-',
      '/Ox',
      '/Ob2',
      '/Oi',
      '/Ot',
      '/Oy',
      '/GL',
      '/D', 'WIN32',
      '/D', '_CRT_SECURE_NO_DEPRECATE',
      '/D', '_CRT_NONSTDC_NO_DEPRECATE',
      '/D', '_HAS_EXCEPTIONS=0',
      '/D', 'BUILDING_V8_SHARED=1',
      '/D', 'BUILDING_UV_SHARED=1',
      '/D', 'BUILDING_NODE_EXTENSION',
      '/D', '_WINDLL',
      '/GF',
      '/Gm-',
      '/MT',
      '/GS',
      '/Gy',
      '/fp:precise',
      '/Zc:wchar_t',
      '/Zc:forScope',
      '/Zc:inline',
      '/GR-',
      '/Gd',
      '/TP',
      '/wd4351',
      '/wd4355',
      '/wd4800',
      '/wd4251',
      '/errorReport:queue',
      '/MP'
    ],
    linker_flags: [
      '/ERRORREPORT:QUEUE',
      '/INCREMENTAL:NO',
      'kernel32.lib', 'user32.lib', 'gdi32.lib', 'winspool.lib ', 'comdlg32.lib',
      'advapi32.lib', 'shell32.lib', 'ole32.lib', 'oleaut32.lib', 'uuid.lib', 'odbc32.lib',
      `${process.cwd()}\\build\\deps\\${process.versions.node}\\node.lib`,
      // '/MANIFEST',
      // `/MANIFESTUAC:"level='asInvoker' uiAccess='false'"`,
      // '/manifest:embed',
      '/MAP', '/MAPINFO:EXPORTS',
      '/OPT:REF',
      '/OPT:ICF',
      '/LTCG',
      '/TLBID:1',
      '/DYNAMICBASE',
      '/NXCOMPAT',
      `/MACHINE:${linkerArch}`,
      '/ignore:4199',
      '/DLL'
     ]
  }
}
