# bufferswap

Used SIMD to improve bufferswap performance including below APIs: 
```
buffer.swap16()
buffer.swap32()
buffer.swap64()
```
Accoring to Node.js buffer API design, only the buffer over 128 bytes
go into C++ level implementations. This optimization only works for this
case. The optimization takes effect on X86 platforms that support AVX512vbmi
instructions, e.g. Cannolake, Icelake, Icelale based AWS EC2 instances.

The code is placed in deps/ folder as platform dependent.

To acheive the best performance, it adopts AVX512 permute instruction
and uses inlining instead of function call because the SIMD logic is too
short.
