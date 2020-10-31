***NOTE*** 

If you get linker errors about undefined references to symbols that involve types in the `std::__cxx11` namespace or the tag
`[abi:cxx11]` then it probably indicates that you are trying to link together object files that were compiled with different
values for the _GLIBCXX_USE_CXX11_ABI marco. This commonly happens when linking to a third-party library that was compiled with 
an older version of GCC. If the third-party library cannot be rebuilt with the new ABI, then you need to recompile your code with
the old ABI,just like:
**g++ stringWrite.cpp -ljsoncpp -std=c++11 -D_GLIBCXX_USE_CXX11_ABI=0 -o stringWrite**

Not all of uses of the new ABI will cause changes in symbol names, for example a class with a `std::string` member variable will
have the same mangled name whether compiled with the older or new ABI. In order to detect such problems, the new types and functions
are annotated with the abi_tag attribute, allowing the compiler to warn about potential ABI incompatibilities in code using them.
Those warnings can be enabled with the `-Wabi-tag` option.
