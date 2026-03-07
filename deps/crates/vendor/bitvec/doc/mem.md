# Memory Element Descriptions

This module describes the memory integers and processor registers used to hold
and manipulate `bitvec` data buffers.

The [`BitRegister`] trait marks the unsigned integers that correspond to
processor registers, and can therefore be used for buffer control. The integers
that are not `BitRegister` can be composed from register values, but are not
able to be used in buffer type parameters.

[`BitRegister`]: self::BitRegister
