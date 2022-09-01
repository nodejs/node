# bufferswap

Used SIMD to improve bufferswap performance including: 
```
buffer.swap16()
buffer.swap32()
buffer.swap64()
```

To acheive the best performance, it adopts AVX512 permute instruction.
This instruction swaps 64 bytes of buffer at one time.