Notes on C-99
=============

This file contains a list of C-99 features we don't allow for OpenSSL.
Starting with 3.6 OpenSSL project is going to gradually adopt C-99 features.
The plan is to bring those features in small steps. Either with new
code where particular C-99 construct makes sense (think of designated initializers),
or when refactoring existing code and using C-99 language feature improves
readability/maintainability of the code.  C-99 seems to be implemented by major
compilers ([clang](https://clang.llvm.org/c_status.html#c99), [gcc](https://gcc.gnu.org/c99status.html), [msvc](https://learn.microsoft.com/en-us/cpp/overview/visual-cpp-language-conformance?view=msvc-170)), therefore we can opt
for permissive policy to adopt C-99 standard. This approach means OpenSSL
project accepts all C-99 features except those explicitly listed here.
The list here is going to be updated by features we either

   - find not useful (or not a good match) for OpenSSL

   - the feature is not implemented by some non-mainstream compiler which
     we need to keep supported for benefit of OpenSSL users

The list of C-99 features we don't support in OpenSSL project follows:

   - do not use `//` for comments, stick to `/* ... */`

   - do not use `<complex.h>`. MSVC doesn't quite implement it to standard.

   - do not use variable length arrays, i.e. arrays where the size is
     determined by another variable.  MSVC doesn't implement it at all.
     For clarity, this is an example of such an array:

     ``` C
     int fun(size_t n)
     {
         char s[n]; /* variable size array */
         ...
     ```
