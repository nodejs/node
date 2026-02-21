Intel® Instrumentation and Tracing Technology (ITT) and Just-In-Time (JIT) API
==================================================================================

This ITT/JIT open source profiling API includes:

  - Instrumentation and Tracing Technology (ITT) API
  - Just-In-Time (JIT) Profiling API

The Instrumentation and Tracing Technology (ITT) API enables your application
to generate and control the collection of trace data during its execution 
across different Intel tools.

ITT API consists of two parts: a _static part_ and a _dynamic part_. The
_dynamic part_ is specific for a tool and distributed only with a particular
tool. The _static part_ is a common part shared between tools. Currently, the
static part of ITT API is distributed as a static library and released under
a BSD/GPLv2 dual license with every tool supporting ITT API.

### Build

To build the library:
 - On Windows, Linux and OSX: requires [cmake](https://cmake.org) to be set in `PATH`
 - Windows: requires Visual Studio installed or requires [Ninja](https://github.com/ninja-build/ninja/releases) to be set in `PATH`
 - To list available build options execute: `python buildall.py -h`
```
usage: buildall.py [-h] [-d] [-c] [-v] [-pt] [--force_bits]

optional arguments:
  -h, --help     show this help message and exit
  -d, --debug    specify debug build configuration (release by default)
  -c, --clean    delete any intermediate and output files
  -v, --verbose  enable verbose output from build process
  -pt, --ptmark  enable anomaly detection support
  --force_bits   specify bit version for the target
```
### License

All code in the repo is dual licensed under GPLv2 and 3-Clause BSD licenses
