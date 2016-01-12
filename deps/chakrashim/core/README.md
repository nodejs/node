# ChakraCore

ChakraCore is the core part of Chakra, the high-performance JavaScript engine that powers Microsoft Edge and Windows applications written in HTML/CSS/JS.  ChakraCore supports Just-in-time (JIT) compilation of JavaScript for x86/x64/ARM, garbage collection, and a wide range of the latest JavaScript features.  ChakraCore also supports the [JavaScript Runtime (JSRT) APIs](https://github.com/Microsoft/ChakraCore/wiki/JavaScript-Runtime-%28JSRT%29-Overview), which allows you to easily embed ChakraCore in your applications.

You can stay up-to-date on progress by following the [MSEdge developer blog](http://blogs.windows.com/msedgedev/).

## Build Status

|         | __Debug__ | __Test__ | __Release__ |
|:-------:|:---------:|:--------:|:-----------:|
| __x86__ | [![x86debug][x86dbgicon]][x86dbglink] | [![x86test][x86testicon]][x86testlink] | [![x86release][x86relicon]][x86rellink] |
| __x64__ | [![x64debug][x64dbgicon]][x64dbglink] | [![x64test][x64testicon]][x64testlink] | [![x64release][x64relicon]][x64rellink] |
| __arm__ | [![armdebug][armdbgicon]][armdbglink] | [![armtest][armtesticon]][armtestlink] | [![armrelease][armrelicon]][armrellink] |

[x86dbgicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x86_debug/badge/icon
[x86dbglink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x86_debug/
[x86testicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x86_test/badge/icon
[x86testlink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x86_test/
[x86relicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x86_release/badge/icon
[x86rellink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x86_release/

[x64dbgicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x64_debug/badge/icon
[x64dbglink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x64_debug/
[x64testicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x64_test/badge/icon
[x64testlink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x64_test/
[x64relicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x64_release/badge/icon
[x64rellink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_x64_release/

[armdbgicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_arm_debug/badge/icon
[armdbglink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_arm_debug/
[armtesticon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_arm_test/badge/icon
[armtestlink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_arm_test/
[armrelicon]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_arm_release/badge/icon
[armrellink]: http://dotnet-ci.cloudapp.net/job/Private/job/Microsoft_ChakraCore_arm_release/

Note: these badges are correct but currently display on GitHub as image not found because of permissions. This will be fixed when the build jobs are public.

## Security

If you believe you have found a security issue in ChakraCore, please share it with us privately following the guidance at the Microsoft [Security TechCenter](https://technet.microsoft.com/en-us/security/ff852094). Reporting it via this channel helps minimize risk to projects built with ChakraCore.

## Documentation

* [ChakraCore Architecture](https://github.com/Microsoft/ChakraCore/wiki/Architecture-Overview)
* [Quickstart Embedding ChakraCore](https://github.com/Microsoft/ChakraCore/wiki/Embedding-ChakraCore)
* [JSRT Reference](https://github.com/Microsoft/ChakraCore/wiki/JavaScript-Runtime-%28JSRT%29-Reference)
* [Contribution guidelines](CONTRIBUTING.md)
* [Blogs, talks and other resources](https://github.com/Microsoft/ChakraCore/wiki/Resources)

## Building ChakraCore

You can build ChakraCore on Windows 7 SP1 or above, and Windows Server 2008 R2 or above, with either Visual Studio 2013 or 2015 with C++ support installed.  Once you have Visual Studio installed:

* Clone ChakraCore through ```git clone https://github.com/Microsoft/ChakraCore.git```
* Open `Build\Chakra.Core.sln` in Visual Studio
* Build Solution

More details in [Building ChakraCore](https://github.com/Microsoft/ChakraCore/wiki/Building-ChakraCore).

## Using ChakraCore

Once built, you have a few options for how you can use ChakraCore:

* The most basic is to test the engine is running correctly with the *ch.exe* binary.  This app is a lightweight hosting of JSRT that you can use to run small applications.  After building, you can find this binary in: `Build\VcBuild\bin\[platform+output]`  (eg. `Build\VcBuild\bin\x64_debug`)
* You can [embed ChakraCore](https://github.com/Microsoft/ChakraCore/wiki/Embedding-ChakraCore) in your applications - see [documentation](https://github.com/Microsoft/ChakraCore/wiki/Embedding-ChakraCore) and [samples](http://aka.ms/chakracoresamples).
* Finally, you can also use ChakraCore as the JavaScript engine in Node.  You can learn more by reading how to use [Chakra as Node's JS engine](https://github.com/Microsoft/node)

_A note about using ChakraCore_: ChakraCore is the foundational JavaScript engine, but it does not include the external APIs that make up the modern JavaScript development experience.  For example, DOM APIs like ```document.write()``` are additional APIs that are not available by default and would need to be provided.  For debugging, you may instead want to use ```print()```.

## Contribute

Contributions to ChakraCore are welcome.  Here is how you can contribute to ChakraCore:

* [Submit bugs](https://github.com/Microsoft/ChakraCore/issues) and help us verify fixes
* [Submit pull requests](https://github.com/Microsoft/ChakraCore/pulls) for bug fixes and features and discuss existing proposals
* Chat about [@ChakraCore](https://twitter.com/ChakraCore) on Twitter

Please refer to [Contribution guidelines](CONTRIBUTING.md) for more details.

## Roadmap
For details on our planned features and future direction please refer to our [roadmap](https://github.com/Microsoft/ChakraCore/wiki/Roadmap).

## Contact us
For questions about ChakraCore, please open an [issue](https://github.com/Microsoft/ChakraCore/issues/new) and prefix the issue title with [Question]. 
