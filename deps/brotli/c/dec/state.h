/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Brotli state for partial streaming decoding. */

#ifndef BROTLI_DEC_STATE_H_
#define BROTLI_DEC_STATE_H_

#include "../common/constants.h"
#include "../common/dictionary.h"
#include "../common/platform.h"
#include "../common/transform.h"
#include <brotli/types.h>
#include "./bit_reader.h"
#include "./huffman.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef enum {
  BROTLI_STATE_UNINITED,
  BROTLI_STATE_LARGE_WINDOW_BITS,
  BROTLI_STATE_INITIALIZE,
  BROTLI_STATE_METABLOCK_BEGIN,
  BROTLI_STATE_METABLOCK_HEADER,
  BROTLI_STATE_METABLOCK_HEADER_2,
  BROTLI_STATE_CONTEXT_MODES,
  BROTLI_STATE_COMMAND_BEGIN,
  BROTLI_STATE_COMMAND_INNER,
  BROTLI_STATE_COMMAND_POST_DECODE_LITERALS,
  BROTLI_STATE_COMMAND_POST_WRAP_COPY,
  BROTLI_STATE_UNCOMPRESSED,
  BROTLI_STATE_METADATA,
  BROTLI_STATE_COMMAND_INNER_WRITE,
  BROTLI_STATE_METABLOCK_DONE,
  BROTLI_STATE_COMMAND_POST_WRITE_1,
  BROTLI_STATE_COMMAND_POST_WRITE_2,
  BROTLI_STATE_HUFFMAN_CODE_0,
  BROTLI_STATE_HUFFMAN_CODE_1,
  BROTLI_STATE_HUFFMAN_CODE_2,
  BROTLI_STATE_HUFFMAN_CODE_3,
  BROTLI_STATE_CONTEXT_MAP_1,
  BROTLI_STATE_CONTEXT_MAP_2,
  BROTLI_STATE_TREE_GROUP,
  BROTLI_STATE_DONE
} BrotliRunningState;

typedef enum {
  BROTLI_STATE_METABLOCK_HEADER_NONE,
  BROTLI_STATE_METABLOCK_HEADER_EMPTY,
  BROTLI_STATE_METABLOCK_HEADER_NIBBLES,
  BROTLI_STATE_METABLOCK_HEADER_SIZE,
  BROTLI_STATE_METABLOCK_HEADER_UNCOMPRESSED,
  BROTLI_STATE_METABLOCK_HEADER_RESERVED,
  BROTLI_STATE_METABLOCK_HEADER_BYTES,
  BROTLI_STATE_METABLOCK_HEADER_METADATA
} BrotliRunningMetablockHeaderState;

typedef enum {
  BROTLI_STATE_UNCOMPRESSED_NONE,
  BROTLI_STATE_UNCOMPRESSED_WRITE
} BrotliRunningUncompressedState;

typedef enum {
  BROTLI_STATE_TREE_GROUP_NONE,
  BROTLI_STATE_TREE_GROUP_LOOP
} BrotliRunningTreeGroupState;

typedef enum {
  BROTLI_STATE_CONTEXT_MAP_NONE,
  BROTLI_STATE_CONTEXT_MAP_READ_PREFIX,
  BROTLI_STATE_CONTEXT_MAP_HUFFMAN,
  BROTLI_STATE_CONTEXT_MAP_DECODE,
  BROTLI_STATE_CONTEXT_MAP_TRANSFORM
} BrotliRunningContextMapState;

typedef enum {
  BROTLI_STATE_HUFFMAN_NONE,
  BROTLI_STATE_HUFFMAN_SIMPLE_SIZE,
  BROTLI_STATE_HUFFMAN_SIMPLE_READ,
  BROTLI_STATE_HUFFMAN_SIMPLE_BUILD,
  BROTLI_STATE_HUFFMAN_COMPLEX,
  BROTLI_STATE_HUFFMAN_LENGTH_SYMBOLS
} BrotliRunningHuffmanState;

typedef enum {
  BROTLI_STATE_DECODE_UINT8_NONE,
  BROTLI_STATE_DECODE_UINT8_SHORT,
  BROTLI_STATE_DECODE_UINT8_LONG
} BrotliRunningDecodeUint8State;

typedef enum {
  BROTLI_STATE_READ_BLOCK_LENGTH_NONE,
  BROTLI_STATE_READ_BLOCK_LENGTH_SUFFIX
} BrotliRunningReadBlockLengthState;

struct BrotliDecoderStateStruct {
  BrotliRunningState state;

  /* This counter is reused for several disjoint loops. */
  int loop_counter;

  BrotliBitReader br;

  brotli_alloc_func alloc_func;
  brotli_free_func free_func;
  void* memory_manager_opaque;

  /* Temporary storage for remaining input. */
  union {
    uint64_t u64;
    uint8_t u8[8];
  } buffer;
  uint32_t buffer_length;

  int pos;
  int max_backward_distance;
  int max_distance;
  int ringbuffer_size;
  int ringbuffer_mask;
  int dist_rb_idx;
  int dist_rb[4];
  int error_code;
  uint32_t sub_loop_counter;
  uint8_t* ringbuffer;
  uint8_t* ringbuffer_end;
  HuffmanCode* htree_command;
  const uint8_t* context_lookup;
  uint8_t* context_map_slice;
  uint8_t* dist_context_map_slice;

  /* This ring buffer holds a few past copy distances that will be used by
     some special distance codes. */
  HuffmanTreeGroup literal_hgroup;
  HuffmanTreeGroup insert_copy_hgroup;
  HuffmanTreeGroup distance_hgroup;
  HuffmanCode* block_type_trees;
  HuffmanCode* block_len_trees;
  /* This is true if the literal context map histogram type always matches the
     block type. It is then not needed to keep the context (faster decoding). */
  int trivial_literal_context;
  /* Distance context is actual after command is decoded and before distance is
     computed. After distance computation it is used as a temporary variable. */
  int distance_context;
  int meta_block_remaining_len;
  uint32_t block_length_index;
  uint32_t block_length[3];
  uint32_t num_block_types[3];
  uint32_t block_type_rb[6];
  uint32_t distance_postfix_bits;
  uint32_t num_direct_distance_codes;
  int distance_postfix_mask;
  uint32_t num_dist_htrees;
  uint8_t* dist_context_map;
  HuffmanCode* literal_htree;
  uint8_t dist_htree_index;
  uint32_t repeat_code_len;
  uint32_t prev_code_len;

  int copy_length;
  int distance_code;

  /* For partial write operations. */
  size_t rb_roundtrips;  /* how many times we went around the ring-buffer */
  size_t partial_pos_out;  /* how much output to the user in total */

  /* For ReadHuffmanCode. */
  uint32_t symbol;
  uint32_t repeat;
  uint32_t space;

  HuffmanCode table[32];
  /* List of heads of symbol chains. */
  uint16_t* symbol_lists;
  /* Storage from symbol_lists. */
  uint16_t symbols_lists_array[BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                               BROTLI_NUM_COMMAND_SYMBOLS];
  /* Tails of symbol chains. */
  int next_symbol[32];
  uint8_t code_length_code_lengths[BROTLI_CODE_LENGTH_CODES];
  /* Population counts for the code lengths. */
  uint16_t code_length_histo[16];

  /* For HuffmanTreeGroupDecode. */
  int htree_index;
  HuffmanCode* next;

  /* For DecodeContextMap. */
  uint32_t context_index;
  uint32_t max_run_length_prefix;
  uint32_t code;
  HuffmanCode context_map_table[BROTLI_HUFFMAN_MAX_SIZE_272];

  /* For InverseMoveToFrontTransform. */
  uint32_t mtf_upper_bound;
  uint32_t mtf[64 + 1];

  /* Less used attributes are at the end of this struct. */

  /* States inside function calls. */
  BrotliRunningMetablockHeaderState substate_metablock_header;
  BrotliRunningTreeGroupState substate_tree_group;
  BrotliRunningContextMapState substate_context_map;
  BrotliRunningUncompressedState substate_uncompressed;
  BrotliRunningHuffmanState substate_huffman;
  BrotliRunningDecodeUint8State substate_decode_uint8;
  BrotliRunningReadBlockLengthState substate_read_block_length;

  unsigned int is_last_metablock : 1;
  unsigned int is_uncompressed : 1;
  unsigned int is_metadata : 1;
  unsigned int should_wrap_ringbuffer : 1;
  unsigned int canny_ringbuffer_allocation : 1;
  unsigned int large_window : 1;
  unsigned int size_nibbles : 8;
  uint32_t window_bits;

  int new_ringbuffer_size;

  uint32_t num_literal_htrees;
  uint8_t* context_map;
  uint8_t* context_modes;

  const BrotliDictionary* dictionary;
  const BrotliTransforms* transforms;

  uint32_t trivial_literal_contexts[8];  /* 256 bits */
};

typedef struct BrotliDecoderStateStruct BrotliDecoderStateInternal;
#define BrotliDecoderState BrotliDecoderStateInternal

BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderStateInit(BrotliDecoderState* s,
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);
BROTLI_INTERNAL void BrotliDecoderStateCleanup(BrotliDecoderState* s);
BROTLI_INTERNAL void BrotliDecoderStateMetablockBegin(BrotliDecoderState* s);
BROTLI_INTERNAL void BrotliDecoderStateCleanupAfterMetablock(
    BrotliDecoderState* s);
BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderHuffmanTreeGroupInit(
    BrotliDecoderState* s, HuffmanTreeGroup* group, uint32_t alphabet_size,
    uint32_t max_symbol, uint32_t ntrees);

#define BROTLI_DECODER_ALLOC(S, L) S->alloc_func(S->memory_manager_opaque, L)

#define BROTLI_DECODER_FREE(S, X) {          \
  S->free_func(S->memory_manager_opaque, X); \
  X = NULL;                                  \
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_DEC_STATE_H_ */
