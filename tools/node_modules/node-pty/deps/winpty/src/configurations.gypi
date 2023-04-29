# By default gyp/msbuild build for 32-bit Windows.  This gyp include file
# defines configurations for both 32-bit and 64-bit Windows.  To use it, run:
#
#     C:\...\winpty\src>gyp -I configurations.gypi
#
# This command generates Visual Studio project files with a Release
# configuration and two Platforms--Win32 and x64.  Both can be built:
#
#     C:\...\winpty\src>msbuild winpty.sln /p:Platform=Win32
#     C:\...\winpty\src>msbuild winpty.sln /p:Platform=x64
#
# The output is placed in:
#
#     C:\...\winpty\src\Release\Win32
#     C:\...\winpty\src\Release\x64
#
# Windows XP note: By default, the project files will use the default "toolset"
# for the given MSVC version.  For MSVC 2013 and MSVC 2015, the default toolset
# generates binaries that do not run on Windows XP.  To target Windows XP,
# select the XP-specific toolset by passing
# -D WINPTY_MSBUILD_TOOLSET={v120_xp,v140_xp} to gyp (v120_xp == MSVC 2013,
# v140_xp == MSVC 2015).  Unfortunately, it isn't possible to have a single
# project file with configurations for both XP and post-XP.  This seems to be a
# limitation of the MSVC project file format.
#
# This file is not included by default, because I suspect it would interfere
# with node-gyp, which has a different system for building 32-vs-64-bit
# binaries.  It uses a common.gypi, and the project files it generates can only
# build a single architecture, the output paths are not differentiated by
# architecture.

{
    'variables': {
        'WINPTY_MSBUILD_TOOLSET%': '',
    },
    'target_defaults': {
        'default_configuration': 'Release_Win32',
        'configurations': {
            'Release_Win32': {
                'msvs_configuration_platform': 'Win32',
            },
            'Release_x64': {
                'msvs_configuration_platform': 'x64',
            },
        },
        'msvs_configuration_attributes': {
            'OutputDirectory': '$(SolutionDir)$(ConfigurationName)\\$(Platform)',
            'IntermediateDirectory': '$(ConfigurationName)\\$(Platform)\\obj\\$(ProjectName)',
        },
        'msvs_settings': {
            'VCLinkerTool': {
                'SubSystem': '1', # /SUBSYSTEM:CONSOLE
            },
            'VCCLCompilerTool': {
                'RuntimeLibrary': '0', # MultiThreaded (/MT)
            },
        },
        'msbuild_toolset' : '<(WINPTY_MSBUILD_TOOLSET)',
    }
}
