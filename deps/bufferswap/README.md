# bufferswap

Used SIMD to improve bufferswap performance including: 
```
buffer.swap16()
buffer.swap32()
buffer.swap64()
```
To limit the scope of the compiling and linking flag of -mavx512, the
code is moved into deps/ folder.

To acheive the best performance, it adopts AVX512 permute instruction
and uses inlining instead of function call because the SIMD logic is too
short.