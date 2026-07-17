#!/usr/bin/env python3
#
# Copyright 2025 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This script generates kCRC32CPowers[]."""


def poly_mul(a, b):
  """Polynomial multiplication: a * b."""
  product = 0
  for i in range(b.bit_length()):
    if (b & (1 << i)) != 0:
      product ^= a << i
  return product


def poly_div(a, b):
  """Polynomial division: floor(a / b)."""
  q = 0
  while a.bit_length() >= b.bit_length():
    q ^= 1 << (a.bit_length() - b.bit_length())
    a ^= b << (a.bit_length() - b.bit_length())
  return q


def poly_reduce(a, b):
  """Polynomial reduction: a mod b."""
  return a ^ poly_mul(poly_div(a, b), b)


def poly_exp(a, b, g):
  """Polynomial exponentiation: a^b mod g."""
  if b == 1:
    return poly_reduce(a, g)
  c = poly_exp(a, b // 2, g)
  c = poly_mul(c, c)
  if b % 2 != 0:
    c = poly_mul(c, a)
  return poly_reduce(c, g)


def bitreflect(a, num_bits):
  """Reflects the bits of the given integer."""
  if a.bit_length() > num_bits:
    raise ValueError(f'Integer has more than {num_bits} bits')
  return sum(((a >> i) & 1) << (num_bits - 1 - i) for i in range(num_bits))


G = 0x11EDC6F41  # The CRC-32C reducing polynomial, in the "natural" bit order
CRC_BITS = 32  # The degree of G, i.e. the 32 in "CRC-32C"
LSB_FIRST = True  # CRC-32C is a least-significant-bit-first CRC
NUM_SIZE_BITS = 64  # The maximum number of bits in the length (size_t)
NUM_DROPPED_BITS = 4  # The number of bits dropped from the length
LOG2_BITS_PER_BYTE = 3  # log2 of the number of bits in a byte, i.e. log2(8)
X = 2  # The polynomial 'x', in the "natural" bit order


def print_crc32c_powers():
  """Generates kCRC32CPowers[].

  kCRC32CPowers[] is an array of length NUM_SIZE_BITS - NUM_DROPPED_BITS,
  whose i'th entry is x^(2^(i + LOG2_BITS_PER_BYTE + NUM_DROPPED_BITS) -
  CRC_BITS - 1) mod G. See kCRC32CPowers[] in the C++ source for more info.
  """
  for i in range(NUM_SIZE_BITS - NUM_DROPPED_BITS):
    poly = poly_exp(
        X,
        2 ** (i + LOG2_BITS_PER_BYTE + NUM_DROPPED_BITS)
        - CRC_BITS
        - (1 if LSB_FIRST else 0),
        G,
    )
    poly = bitreflect(poly, CRC_BITS)
    print(f'0x{poly:0{2*CRC_BITS//8}x}, ', end='')


if __name__ == '__main__':
  print_crc32c_powers()
