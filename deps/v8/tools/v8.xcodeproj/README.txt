The Xcode project for V8 has been retired. If an Xcode project
is needed for building on a Mac there is the option of using GYP to
generate it. Please look in the build directory in the root of the
V8 project. It contains the required infrastructure and a README.txt
file explaining how to get started.

Generating Xcode projects using GYP is how the Chromium
project integrated V8 into the Mac build.

The main build system for V8 is still SCons, see
http://code.google.com/apis/v8/build.html for details.
