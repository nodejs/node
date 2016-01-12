//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// API for methods that the ConfigParser can call
// Implement these methods to customize what happens when the configuration is loaded
class ConfigParserAPI
{
public:
    // If the -Console flag is passed in, this method is called with a buffer
    // The implementor will fill in the buffer and return true if a custom Console window title is desired
    // To not change the console title, return false
    static bool FillConsoleTitle(__ecount(cchBufferSize) LPWSTR buffer, size_t cchBufferSize, __in LPWSTR moduleName);

    // If one of the following flags:
    //  - Console
    //  - OutputFile
    //  - DebugWindow
    //  - InMemoryTrace
    // is set, then the ConfigParser will call the following method to give the implementor
    // a chance to output any headers at initialization time.
    static void DisplayInitialOutput(__in LPWSTR moduleName);
};

class ConfigParser
{
private:
    static const int  MaxTokenSize  = 512;
    static const int MaxRegSize = 2048;
    static const LPWSTR featureKeyName ;
    bool _hasReadConfig;
    Js::ConfigFlagsTable& _flags;
    const LPCWSTR _configFileName;

    // NT version of CRT has the "backward compat" behavior that returns 0 instead of EOF
    // for unicode version of fwscanf.
#ifdef NTBUILD
    static const int FINISHED = 0;
#else
    static const int FINISHED = EOF;
#endif

    void ParseRegistryKey(HKEY hk, CmdLineArgsParser &parser);

public:
    static ConfigParser s_moduleConfigParser;

    ConfigParser(Js::ConfigFlagsTable& outputFlags, __in LPCWSTR configFileName = L"jscript") :
        _flags(outputFlags),
        _hasReadConfig(false),
        _configFileName(configFileName)
    {
    }

    static void ParseOnModuleLoad(CmdLineArgsParser& parser, HANDLE hMod);

    void ParseConfig(HANDLE hmod, CmdLineArgsParser &parser);
    void ParseRegistry(CmdLineArgsParser &parser);
    void ProcessConfiguration(HANDLE mod);
    HRESULT SetOutputFile(const WCHAR* outputFile, const WCHAR* openMode);
    bool HasReadConfig() { return _hasReadConfig; }
};
