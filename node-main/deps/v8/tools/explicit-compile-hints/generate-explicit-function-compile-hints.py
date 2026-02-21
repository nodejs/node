#!/usr/bin/env python3

# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import sys

varint_mask = (1 << 5) - 1
varint_continue_bit = 1 << 5
varint_shift = 5

# We use the same VLQ-Base64 encoding as source maps, where each integer is
# first expressed as a variable length sequence of 6-bit items, and each item
# is then Base64-encoded individually.

# Index -> character
custom_base64_encoding = [0] * 64
# Character -> index
custom_base64_decoding = [-1] * 256


def fill_custom_base64_data():
  base = 0
  for i in range(ord('A'), ord('Z') + 1):
    custom_base64_encoding[base + i - ord('A')] = chr(i)
  base += 26
  for i in range(ord('a'), ord('z') + 1):
    custom_base64_encoding[base + i - ord('a')] = chr(i)
  base += 26
  for i in range(ord('0'), ord('9') + 1):
    custom_base64_encoding[base + i - ord('0')] = chr(i)
  base += 10
  custom_base64_encoding[base] = '+'
  custom_base64_encoding[base + 1] = '/'

  for i in range(0, 64):
    custom_base64_decoding[ord(custom_base64_encoding[i])] = i


fill_custom_base64_data()


def encode_varint(n):
  # Encodes integer n as a varint the same way as in source maps.

  # Add sign bit (the least significant bit):
  if n < 0:
    n = ((-n) << 1) | 1
  else:
    n = n << 1

  # Each item represents varint_shift bits; the first item represents the
  # least significant bits etc.
  res = []
  while n > 0:
    current_item = n & varint_mask
    n = n >> varint_shift
    if n > 0:
      current_item = current_item | varint_continue_bit
    res.append(current_item)
  return res


def custom_b64encode(items):
  result = []
  for item in items:
    result.append(custom_base64_encoding[item])
  return ''.join(result)


def custom_b64decode(s):
  result = []
  for c in s:
    index = custom_base64_decoding[ord(c)]
    if index < 0 or index > 64:
      raise 'Corrupt data'
    result.append(index)
  return result


def encode_delta_varint_base64(start_positions):
  bytes = []
  prev_pos = 0
  for p in start_positions:
    bytes.extend(encode_varint(p - prev_pos))
    prev_pos = p
  return custom_b64encode(bytes)


def decode_varints(bytes):
  ints = []
  current = 0
  byte_ix = 0
  while byte_ix < len(bytes):
    # Start parsing the new varint.
    n = bytes[byte_ix]
    byte_ix += 1

    current = n & varint_mask
    current_shift = 0

    while n & varint_continue_bit != 0:
      # Read another item.
      if byte_ix >= len(bytes):
        raise 'Corrupt data'
      n = bytes[byte_ix]
      byte_ix += 1
      current_shift = current_shift + varint_shift
      current = current | ((n & varint_mask) << current_shift)

    if current & 1 != 0:
      sign = -1
    else:
      sign = 1
    current = sign * (current >> 1)
    ints.append(current)
  return ints


def decode_delta_varint_base64(comment):
  bytes = custom_b64decode(comment)
  deltas = decode_varints(bytes)
  prev_pos = 0
  positions = []
  for d in deltas:
    prev_pos += d
    positions.append(prev_pos)
  return positions


def generate_compile_hints(script_name_to_start_positions):
  for script_name, start_positions in script_name_to_start_positions.items():
    start_positions = list(start_positions)
    start_positions.sort()
    print(script_name)
    magic_comment = encode_delta_varint_base64(start_positions)
    back = decode_delta_varint_base64(magic_comment)
    if start_positions != back:
      print('Decoding failed!')
      exit(1)
    print(f"//# functionsCalledOnLoad={magic_comment}\n")


def process_v8_log(filename):
  script_id_to_name = {}
  script_name_to_start_positions = {}
  with open(filename, 'r') as log_file:
    for line in log_file:
      fields = line.strip().split(',')

      if len(fields) >= 3:
        if fields[0] == 'script-details':
          script_id = int(fields[1])
          script_name = fields[2]

          script_id_to_name[script_id] = script_name
        elif fields[0] == 'function' and fields[1] == 'parse-function':
          script_id = int(fields[2])
          start_position = int(fields[3])
          script_name = script_id_to_name[script_id]

          if script_name in script_name_to_start_positions:
            script_name_to_start_positions[script_name].add(start_position)
          else:
            script_name_to_start_positions[script_name] = {start_position}
    generate_compile_hints(script_name_to_start_positions)


def process_text(filename):
  # Format:
  # file1, pos1
  # file2, pos2
  # file1, pos1
  # ...
  script_name_to_start_positions = {}
  with open(filename, 'r') as log_file:
    for line in log_file:
      fields = line.strip().split(',')
      if len(fields) >= 2:
        script_name = fields[0]
        start_position = int(fields[1])
        if script_name in script_name_to_start_positions:
          script_name_to_start_positions[script_name].add(start_position)
        else:
          script_name_to_start_positions[script_name] = {start_position}
    generate_compile_hints(script_name_to_start_positions)


def print_usage():
  print(
      'Usage: python3 generate-explicit-function-compile-hints.py --v8log v8.log'
  )
  print(
      'or:    python3 generate-explicit-function-compile-hints.py --text data.txt'
  )


def main():
  if len(sys.argv) < 3 or '--help' in sys.argv:
    print_usage()
    exit(1)
  if sys.argv[1] == '--v8log':
    process_v8_log(sys.argv[2])
  elif sys.argv[1] == '--text':
    process_text(sys.argv[2])
  else:
    print_usage()
    exit(1)


if __name__ == '__main__':
  main()
