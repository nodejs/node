#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "llhttp.h"

typedef int (*llhttp__internal__span_cb)(
             llhttp__internal_t*, const char*, const char*);

static const unsigned char llparse_blob0[] = {
  'C', 'L'
};
static const unsigned char llparse_blob1[] = {
  'o', 'n'
};
static const unsigned char llparse_blob2[] = {
  'e', 'c', 't', 'i', 'o', 'n'
};
static const unsigned char llparse_blob3[] = {
  'l', 'o', 's', 'e'
};
static const unsigned char llparse_blob4[] = {
  'e', 'e', 'p', '-', 'a', 'l', 'i', 'v', 'e'
};
static const unsigned char llparse_blob5[] = {
  'p', 'g', 'r', 'a', 'd', 'e'
};
static const unsigned char llparse_blob6[] = {
  'h', 'u', 'n', 'k', 'e', 'd'
};
static const unsigned char llparse_blob7[] = {
  'e', 'n', 't', '-', 'l', 'e', 'n', 'g', 't', 'h'
};
static const unsigned char llparse_blob8[] = {
  'r', 'o', 'x', 'y', '-', 'c', 'o', 'n', 'n', 'e', 'c',
  't', 'i', 'o', 'n'
};
static const unsigned char llparse_blob9[] = {
  'r', 'a', 'n', 's', 'f', 'e', 'r', '-', 'e', 'n', 'c',
  'o', 'd', 'i', 'n', 'g'
};
static const unsigned char llparse_blob10[] = {
  'p', 'g', 'r', 'a', 'd', 'e'
};
static const unsigned char llparse_blob11[] = {
  0xd, 0xa
};
static const unsigned char llparse_blob12[] = {
  'T', 'T', 'P', '/'
};
static const unsigned char llparse_blob13[] = {
  'C', 'E', '/'
};
static const unsigned char llparse_blob14[] = {
  'I', 'N', 'D'
};
static const unsigned char llparse_blob15[] = {
  'E', 'C', 'K', 'O', 'U', 'T'
};
static const unsigned char llparse_blob16[] = {
  'N', 'E', 'C', 'T'
};
static const unsigned char llparse_blob17[] = {
  'E', 'L', 'E', 'T', 'E'
};
static const unsigned char llparse_blob18[] = {
  'E', 'T'
};
static const unsigned char llparse_blob19[] = {
  'E', 'A', 'D'
};
static const unsigned char llparse_blob20[] = {
  'N', 'K'
};
static const unsigned char llparse_blob21[] = {
  'C', 'K'
};
static const unsigned char llparse_blob22[] = {
  'S', 'E', 'A', 'R', 'C', 'H'
};
static const unsigned char llparse_blob23[] = {
  'R', 'G', 'E'
};
static const unsigned char llparse_blob24[] = {
  'C', 'T', 'I', 'V', 'I', 'T', 'Y'
};
static const unsigned char llparse_blob25[] = {
  'L', 'E', 'N', 'D', 'A', 'R'
};
static const unsigned char llparse_blob26[] = {
  'V', 'E'
};
static const unsigned char llparse_blob27[] = {
  'O', 'T', 'I', 'F', 'Y'
};
static const unsigned char llparse_blob28[] = {
  'P', 'T', 'I', 'O', 'N', 'S'
};
static const unsigned char llparse_blob29[] = {
  'T', 'C', 'H'
};
static const unsigned char llparse_blob30[] = {
  'S', 'T'
};
static const unsigned char llparse_blob31[] = {
  'O', 'P'
};
static const unsigned char llparse_blob32[] = {
  'I', 'N', 'D'
};
static const unsigned char llparse_blob33[] = {
  'A', 'T', 'C', 'H'
};
static const unsigned char llparse_blob34[] = {
  'G', 'E'
};
static const unsigned char llparse_blob35[] = {
  'I', 'N', 'D'
};
static const unsigned char llparse_blob36[] = {
  'O', 'R', 'T'
};
static const unsigned char llparse_blob37[] = {
  'A', 'R', 'C', 'H'
};
static const unsigned char llparse_blob38[] = {
  'U', 'R', 'C', 'E'
};
static const unsigned char llparse_blob39[] = {
  'B', 'S', 'C', 'R', 'I', 'B', 'E'
};
static const unsigned char llparse_blob40[] = {
  'R', 'A', 'C', 'E'
};
static const unsigned char llparse_blob41[] = {
  'I', 'N', 'D'
};
static const unsigned char llparse_blob42[] = {
  'N', 'K'
};
static const unsigned char llparse_blob43[] = {
  'C', 'K'
};
static const unsigned char llparse_blob44[] = {
  'U', 'B', 'S', 'C', 'R', 'I', 'B', 'E'
};
static const unsigned char llparse_blob45[] = {
  'H', 'T', 'T', 'P', '/'
};
static const unsigned char llparse_blob46[] = {
  'A', 'D'
};
static const unsigned char llparse_blob47[] = {
  'T', 'P', '/'
};

enum llparse_match_status_e {
  kMatchComplete,
  kMatchPause,
  kMatchMismatch
};
typedef enum llparse_match_status_e llparse_match_status_t;

struct llparse_match_s {
  llparse_match_status_t status;
  const unsigned char* current;
};
typedef struct llparse_match_s llparse_match_t;

static llparse_match_t llparse__match_sequence_id(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp,
    const unsigned char* seq, uint32_t seq_len) {
  uint32_t index;
  llparse_match_t res;

  index = s->_index;
  for (; p != endp; p++) {
    unsigned char current;

    current = *p;
    if (current == seq[index]) {
      if (++index == seq_len) {
        res.status = kMatchComplete;
        goto reset;
      }
    } else {
      res.status = kMatchMismatch;
      goto reset;
    }
  }
  s->_index = index;
  res.status = kMatchPause;
  res.current = p;
  return res;
reset:
  s->_index = 0;
  res.current = p;
  return res;
}

static llparse_match_t llparse__match_sequence_to_lower_unsafe(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp,
    const unsigned char* seq, uint32_t seq_len) {
  uint32_t index;
  llparse_match_t res;

  index = s->_index;
  for (; p != endp; p++) {
    unsigned char current;

    current = ((*p) | 0x20);
    if (current == seq[index]) {
      if (++index == seq_len) {
        res.status = kMatchComplete;
        goto reset;
      }
    } else {
      res.status = kMatchMismatch;
      goto reset;
    }
  }
  s->_index = index;
  res.status = kMatchPause;
  res.current = p;
  return res;
reset:
  s->_index = 0;
  res.current = p;
  return res;
}

enum llparse_state_e {
  s_error,
  s_n_llhttp__internal__n_invoke_llhttp__after_message_complete,
  s_n_llhttp__internal__n_pause_1,
  s_n_llhttp__internal__n_invoke_is_equal_upgrade,
  s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_2,
  s_n_llhttp__internal__n_chunk_data_almost_done_skip,
  s_n_llhttp__internal__n_chunk_data_almost_done,
  s_n_llhttp__internal__n_consume_content_length,
  s_n_llhttp__internal__n_span_start_llhttp__on_body,
  s_n_llhttp__internal__n_invoke_is_equal_content_length,
  s_n_llhttp__internal__n_chunk_size_almost_done,
  s_n_llhttp__internal__n_chunk_parameters,
  s_n_llhttp__internal__n_chunk_size_otherwise,
  s_n_llhttp__internal__n_chunk_size,
  s_n_llhttp__internal__n_invoke_update_content_length,
  s_n_llhttp__internal__n_consume_content_length_1,
  s_n_llhttp__internal__n_span_start_llhttp__on_body_1,
  s_n_llhttp__internal__n_eof,
  s_n_llhttp__internal__n_span_start_llhttp__on_body_2,
  s_n_llhttp__internal__n_invoke_llhttp__after_headers_complete,
  s_n_llhttp__internal__n_headers_almost_done,
  s_n_llhttp__internal__n_span_start_llhttp__on_header_value,
  s_n_llhttp__internal__n_header_value_discard_lws,
  s_n_llhttp__internal__n_header_value_discard_ws_almost_done,
  s_n_llhttp__internal__n_header_value_lws,
  s_n_llhttp__internal__n_header_value_almost_done,
  s_n_llhttp__internal__n_header_value_otherwise,
  s_n_llhttp__internal__n_header_value_connection_token,
  s_n_llhttp__internal__n_header_value_connection_ws,
  s_n_llhttp__internal__n_header_value_connection_1,
  s_n_llhttp__internal__n_header_value_connection_2,
  s_n_llhttp__internal__n_header_value_connection_3,
  s_n_llhttp__internal__n_header_value_connection,
  s_n_llhttp__internal__n_error_14,
  s_n_llhttp__internal__n_header_value,
  s_n_llhttp__internal__n_header_value_discard_rws,
  s_n_llhttp__internal__n_error_15,
  s_n_llhttp__internal__n_header_value_content_length_ws,
  s_n_llhttp__internal__n_header_value_content_length,
  s_n_llhttp__internal__n_header_value_te_chunked_1,
  s_n_llhttp__internal__n_header_value_te_chunked,
  s_n_llhttp__internal__n_span_start_llhttp__on_header_value_1,
  s_n_llhttp__internal__n_header_value_discard_ws,
  s_n_llhttp__internal__n_header_field_general_otherwise,
  s_n_llhttp__internal__n_header_field_general,
  s_n_llhttp__internal__n_header_field_colon,
  s_n_llhttp__internal__n_header_field_3,
  s_n_llhttp__internal__n_header_field_4,
  s_n_llhttp__internal__n_header_field_2,
  s_n_llhttp__internal__n_header_field_1,
  s_n_llhttp__internal__n_header_field_5,
  s_n_llhttp__internal__n_header_field_6,
  s_n_llhttp__internal__n_header_field_7,
  s_n_llhttp__internal__n_header_field,
  s_n_llhttp__internal__n_span_start_llhttp__on_header_field,
  s_n_llhttp__internal__n_header_field_start,
  s_n_llhttp__internal__n_url_skip_to_http09,
  s_n_llhttp__internal__n_url_skip_lf_to_http09,
  s_n_llhttp__internal__n_req_http_end_1,
  s_n_llhttp__internal__n_req_http_end,
  s_n_llhttp__internal__n_req_http_minor,
  s_n_llhttp__internal__n_req_http_dot,
  s_n_llhttp__internal__n_req_http_major,
  s_n_llhttp__internal__n_req_http_start_1,
  s_n_llhttp__internal__n_req_http_start_2,
  s_n_llhttp__internal__n_req_http_start,
  s_n_llhttp__internal__n_url_skip_to_http,
  s_n_llhttp__internal__n_url_fragment,
  s_n_llhttp__internal__n_span_end_stub_query_3,
  s_n_llhttp__internal__n_url_query,
  s_n_llhttp__internal__n_url_query_or_fragment,
  s_n_llhttp__internal__n_url_path,
  s_n_llhttp__internal__n_span_start_stub_path_2,
  s_n_llhttp__internal__n_span_start_stub_path,
  s_n_llhttp__internal__n_span_start_stub_path_1,
  s_n_llhttp__internal__n_url_server_with_at,
  s_n_llhttp__internal__n_url_server,
  s_n_llhttp__internal__n_url_schema_delim_1,
  s_n_llhttp__internal__n_url_schema_delim,
  s_n_llhttp__internal__n_span_end_stub_schema,
  s_n_llhttp__internal__n_url_schema,
  s_n_llhttp__internal__n_url_start,
  s_n_llhttp__internal__n_span_start_llhttp__on_url_1,
  s_n_llhttp__internal__n_span_start_llhttp__on_url,
  s_n_llhttp__internal__n_req_spaces_before_url,
  s_n_llhttp__internal__n_req_first_space_before_url,
  s_n_llhttp__internal__n_start_req_1,
  s_n_llhttp__internal__n_start_req_2,
  s_n_llhttp__internal__n_start_req_4,
  s_n_llhttp__internal__n_start_req_6,
  s_n_llhttp__internal__n_start_req_7,
  s_n_llhttp__internal__n_start_req_5,
  s_n_llhttp__internal__n_start_req_3,
  s_n_llhttp__internal__n_start_req_8,
  s_n_llhttp__internal__n_start_req_9,
  s_n_llhttp__internal__n_start_req_10,
  s_n_llhttp__internal__n_start_req_12,
  s_n_llhttp__internal__n_start_req_13,
  s_n_llhttp__internal__n_start_req_11,
  s_n_llhttp__internal__n_start_req_15,
  s_n_llhttp__internal__n_start_req_16,
  s_n_llhttp__internal__n_start_req_18,
  s_n_llhttp__internal__n_start_req_20,
  s_n_llhttp__internal__n_start_req_21,
  s_n_llhttp__internal__n_start_req_19,
  s_n_llhttp__internal__n_start_req_17,
  s_n_llhttp__internal__n_start_req_22,
  s_n_llhttp__internal__n_start_req_14,
  s_n_llhttp__internal__n_start_req_23,
  s_n_llhttp__internal__n_start_req_24,
  s_n_llhttp__internal__n_start_req_26,
  s_n_llhttp__internal__n_start_req_27,
  s_n_llhttp__internal__n_start_req_30,
  s_n_llhttp__internal__n_start_req_31,
  s_n_llhttp__internal__n_start_req_29,
  s_n_llhttp__internal__n_start_req_28,
  s_n_llhttp__internal__n_start_req_33,
  s_n_llhttp__internal__n_start_req_32,
  s_n_llhttp__internal__n_start_req_25,
  s_n_llhttp__internal__n_start_req_36,
  s_n_llhttp__internal__n_start_req_37,
  s_n_llhttp__internal__n_start_req_35,
  s_n_llhttp__internal__n_start_req_34,
  s_n_llhttp__internal__n_start_req_39,
  s_n_llhttp__internal__n_start_req_40,
  s_n_llhttp__internal__n_start_req_41,
  s_n_llhttp__internal__n_start_req_38,
  s_n_llhttp__internal__n_start_req_42,
  s_n_llhttp__internal__n_start_req_45,
  s_n_llhttp__internal__n_start_req_47,
  s_n_llhttp__internal__n_start_req_48,
  s_n_llhttp__internal__n_start_req_46,
  s_n_llhttp__internal__n_start_req_49,
  s_n_llhttp__internal__n_start_req_44,
  s_n_llhttp__internal__n_start_req_43,
  s_n_llhttp__internal__n_start_req,
  s_n_llhttp__internal__n_res_line_almost_done,
  s_n_llhttp__internal__n_res_status,
  s_n_llhttp__internal__n_span_start_llhttp__on_status,
  s_n_llhttp__internal__n_res_status_start,
  s_n_llhttp__internal__n_res_status_code_otherwise,
  s_n_llhttp__internal__n_res_status_code,
  s_n_llhttp__internal__n_res_http_end,
  s_n_llhttp__internal__n_res_http_minor,
  s_n_llhttp__internal__n_res_http_dot,
  s_n_llhttp__internal__n_res_http_major,
  s_n_llhttp__internal__n_start_res,
  s_n_llhttp__internal__n_req_or_res_method_2,
  s_n_llhttp__internal__n_req_or_res_method_3,
  s_n_llhttp__internal__n_req_or_res_method_1,
  s_n_llhttp__internal__n_req_or_res_method,
  s_n_llhttp__internal__n_start_req_or_res,
  s_n_llhttp__internal__n_invoke_load_type,
  s_n_llhttp__internal__n_start,
};
typedef enum llparse_state_e llparse_state_t;

int llhttp__on_url(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__on_header_field(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__on_header_value(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__on_body(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__on_status(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__internal__c_update_finish(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->finish = 2;
  return 0;
}

int llhttp__on_message_begin(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__internal__c_load_type(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return state->type;
}

int llhttp__internal__c_store_method(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp,
    int match) {
  state->method = match;
  return 0;
}

int llhttp__internal__c_is_equal_method(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return state->method == 5;
}

int llhttp__internal__c_update_http_major(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->http_major = 0;
  return 0;
}

int llhttp__internal__c_update_http_minor(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->http_minor = 9;
  return 0;
}

int llhttp__internal__c_test_flags(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return (state->flags & 128) == 128;
}

int llhttp__on_chunk_complete(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__on_message_complete(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__internal__c_is_equal_upgrade(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return state->upgrade == 1;
}

int llhttp__after_message_complete(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__internal__c_test_flags_1(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return (state->flags & 40) == 40;
}

int llhttp__before_headers_complete(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__on_headers_complete(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__after_headers_complete(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__internal__c_update_content_length(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->content_length = 0;
  return 0;
}

int llhttp__internal__c_mul_add_content_length(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp,
    int match) {
  /* Multiplication overflow */
  if (state->content_length > 0xffffffffffffffffULL / 16) {
    return 1;
  }
  
  state->content_length *= 16;
  
  /* Addition overflow */
  if (match >= 0) {
    if (state->content_length > 0xffffffffffffffffULL - match) {
      return 1;
    }
  } else {
    if (state->content_length < 0ULL - match) {
      return 1;
    }
  }
  state->content_length += match;
  return 0;
}

int llhttp__on_chunk_header(
    llhttp__internal_t* s, const unsigned char* p,
    const unsigned char* endp);

int llhttp__internal__c_is_equal_content_length(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return state->content_length == 0;
}

int llhttp__internal__c_or_flags(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 128;
  return 0;
}

int llhttp__internal__c_update_finish_1(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->finish = 1;
  return 0;
}

int llhttp__internal__c_or_flags_1(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 64;
  return 0;
}

int llhttp__internal__c_update_upgrade(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->upgrade = 1;
  return 0;
}

int llhttp__internal__c_store_header_state(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp,
    int match) {
  state->header_state = match;
  return 0;
}

int llhttp__internal__c_load_header_state(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return state->header_state;
}

int llhttp__internal__c_or_flags_3(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 1;
  return 0;
}

int llhttp__internal__c_update_header_state(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->header_state = 1;
  return 0;
}

int llhttp__internal__c_or_flags_4(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 2;
  return 0;
}

int llhttp__internal__c_or_flags_5(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 4;
  return 0;
}

int llhttp__internal__c_or_flags_6(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 8;
  return 0;
}

int llhttp__internal__c_update_header_state_2(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->header_state = 6;
  return 0;
}

int llhttp__internal__c_update_header_state_4(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->header_state = 0;
  return 0;
}

int llhttp__internal__c_update_header_state_5(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->header_state = 5;
  return 0;
}

int llhttp__internal__c_update_header_state_6(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->header_state = 7;
  return 0;
}

int llhttp__internal__c_test_flags_2(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return (state->flags & 32) == 32;
}

int llhttp__internal__c_mul_add_content_length_1(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp,
    int match) {
  /* Multiplication overflow */
  if (state->content_length > 0xffffffffffffffffULL / 10) {
    return 1;
  }
  
  state->content_length *= 10;
  
  /* Addition overflow */
  if (match >= 0) {
    if (state->content_length > 0xffffffffffffffffULL - match) {
      return 1;
    }
  } else {
    if (state->content_length < 0ULL - match) {
      return 1;
    }
  }
  state->content_length += match;
  return 0;
}

int llhttp__internal__c_or_flags_15(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 32;
  return 0;
}

int llhttp__internal__c_update_header_state_8(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->header_state = 8;
  return 0;
}

int llhttp__internal__c_or_flags_16(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->flags |= 16;
  return 0;
}

int llhttp__internal__c_store_http_major(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp,
    int match) {
  state->http_major = match;
  return 0;
}

int llhttp__internal__c_store_http_minor(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp,
    int match) {
  state->http_minor = match;
  return 0;
}

int llhttp__internal__c_is_equal_method_1(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  return state->method == 33;
}

int llhttp__internal__c_update_status_code(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->status_code = 0;
  return 0;
}

int llhttp__internal__c_mul_add_status_code(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp,
    int match) {
  /* Multiplication overflow */
  if (state->status_code > 0xffff / 10) {
    return 1;
  }
  
  state->status_code *= 10;
  
  /* Addition overflow */
  if (match >= 0) {
    if (state->status_code > 0xffff - match) {
      return 1;
    }
  } else {
    if (state->status_code < 0 - match) {
      return 1;
    }
  }
  state->status_code += match;
  
  /* Enforce maximum */
  if (state->status_code > 999) {
    return 1;
  }
  return 0;
}

int llhttp__internal__c_update_type(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->type = 1;
  return 0;
}

int llhttp__internal__c_update_type_1(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  state->type = 2;
  return 0;
}

int llhttp__internal_init(llhttp__internal_t* state) {
  memset(state, 0, sizeof(*state));
  state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_start;
  return 0;
}

static llparse_state_t llhttp__internal__run(
    llhttp__internal_t* state,
    const unsigned char* p,
    const unsigned char* endp) {
  int match;
  switch ((llparse_state_t) (intptr_t) state->_current) {
    case s_n_llhttp__internal__n_invoke_llhttp__after_message_complete:
    s_n_llhttp__internal__n_invoke_llhttp__after_message_complete: {
      switch (llhttp__after_message_complete(state, p, endp)) {
        default:
          goto s_n_llhttp__internal__n_start;
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_pause_1:
    s_n_llhttp__internal__n_pause_1: {
      state->error = 0x15;
      state->reason = "Pause on CONNECT/Upgrade";
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_llhttp__after_message_complete;
      return s_error;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_invoke_is_equal_upgrade:
    s_n_llhttp__internal__n_invoke_is_equal_upgrade: {
      switch (llhttp__internal__c_is_equal_upgrade(state, p, endp)) {
        case 0:
          goto s_n_llhttp__internal__n_invoke_llhttp__after_message_complete;
        default:
          goto s_n_llhttp__internal__n_pause_1;
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_2:
    s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_2: {
      switch (llhttp__on_message_complete(state, p, endp)) {
        case 0:
          goto s_n_llhttp__internal__n_invoke_is_equal_upgrade;
        case 20:
          goto s_n_llhttp__internal__n_pause_5;
        default:
          goto s_n_llhttp__internal__n_error_8;
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_chunk_data_almost_done_skip:
    s_n_llhttp__internal__n_chunk_data_almost_done_skip: {
      if (p == endp) {
        return s_n_llhttp__internal__n_chunk_data_almost_done_skip;
      }
      p++;
      goto s_n_llhttp__internal__n_invoke_llhttp__on_chunk_complete;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_chunk_data_almost_done:
    s_n_llhttp__internal__n_chunk_data_almost_done: {
      if (p == endp) {
        return s_n_llhttp__internal__n_chunk_data_almost_done;
      }
      p++;
      goto s_n_llhttp__internal__n_chunk_data_almost_done_skip;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_consume_content_length:
    s_n_llhttp__internal__n_consume_content_length: {
      size_t avail;
      size_t need;
      
      avail = endp - p;
      need = state->content_length;
      if (avail >= need) {
        p += need;
        state->content_length = 0;
        goto s_n_llhttp__internal__n_span_end_llhttp__on_body;
      }
      
      state->content_length -= avail;
      return s_n_llhttp__internal__n_consume_content_length;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_body:
    s_n_llhttp__internal__n_span_start_llhttp__on_body: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_body;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_body;
      goto s_n_llhttp__internal__n_consume_content_length;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_invoke_is_equal_content_length:
    s_n_llhttp__internal__n_invoke_is_equal_content_length: {
      switch (llhttp__internal__c_is_equal_content_length(state, p, endp)) {
        case 0:
          goto s_n_llhttp__internal__n_span_start_llhttp__on_body;
        default:
          goto s_n_llhttp__internal__n_invoke_or_flags;
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_chunk_size_almost_done:
    s_n_llhttp__internal__n_chunk_size_almost_done: {
      if (p == endp) {
        return s_n_llhttp__internal__n_chunk_size_almost_done;
      }
      p++;
      goto s_n_llhttp__internal__n_invoke_llhttp__on_chunk_header;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_chunk_parameters:
    s_n_llhttp__internal__n_chunk_parameters: {
      if (p == endp) {
        return s_n_llhttp__internal__n_chunk_parameters;
      }
      switch (*p) {
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_chunk_size_almost_done;
        }
        default: {
          p++;
          goto s_n_llhttp__internal__n_chunk_parameters;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_chunk_size_otherwise:
    s_n_llhttp__internal__n_chunk_size_otherwise: {
      if (p == endp) {
        return s_n_llhttp__internal__n_chunk_size_otherwise;
      }
      switch (*p) {
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_chunk_size_almost_done;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_chunk_parameters;
        }
        case ';': {
          p++;
          goto s_n_llhttp__internal__n_chunk_parameters;
        }
        default: {
          goto s_n_llhttp__internal__n_error_7;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_chunk_size:
    s_n_llhttp__internal__n_chunk_size: {
      if (p == endp) {
        return s_n_llhttp__internal__n_chunk_size;
      }
      switch (*p) {
        case '0': {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '1': {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '2': {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '3': {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '4': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '5': {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '6': {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '7': {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '8': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case '9': {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'A': {
          p++;
          match = 10;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'B': {
          p++;
          match = 11;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'C': {
          p++;
          match = 12;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'D': {
          p++;
          match = 13;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'E': {
          p++;
          match = 14;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'F': {
          p++;
          match = 15;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'a': {
          p++;
          match = 10;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'b': {
          p++;
          match = 11;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'c': {
          p++;
          match = 12;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'd': {
          p++;
          match = 13;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'e': {
          p++;
          match = 14;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        case 'f': {
          p++;
          match = 15;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length;
        }
        default: {
          goto s_n_llhttp__internal__n_chunk_size_otherwise;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_invoke_update_content_length:
    s_n_llhttp__internal__n_invoke_update_content_length: {
      switch (llhttp__internal__c_update_content_length(state, p, endp)) {
        default:
          goto s_n_llhttp__internal__n_chunk_size;
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_consume_content_length_1:
    s_n_llhttp__internal__n_consume_content_length_1: {
      size_t avail;
      size_t need;
      
      avail = endp - p;
      need = state->content_length;
      if (avail >= need) {
        p += need;
        state->content_length = 0;
        goto s_n_llhttp__internal__n_span_end_llhttp__on_body_1;
      }
      
      state->content_length -= avail;
      return s_n_llhttp__internal__n_consume_content_length_1;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_body_1:
    s_n_llhttp__internal__n_span_start_llhttp__on_body_1: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_body_1;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_body;
      goto s_n_llhttp__internal__n_consume_content_length_1;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_eof:
    s_n_llhttp__internal__n_eof: {
      if (p == endp) {
        return s_n_llhttp__internal__n_eof;
      }
      p++;
      goto s_n_llhttp__internal__n_eof;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_body_2:
    s_n_llhttp__internal__n_span_start_llhttp__on_body_2: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_body_2;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_body;
      goto s_n_llhttp__internal__n_invoke_update_finish_1;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_invoke_llhttp__after_headers_complete:
    s_n_llhttp__internal__n_invoke_llhttp__after_headers_complete: {
      switch (llhttp__after_headers_complete(state, p, endp)) {
        case 1:
          goto s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_1;
        case 2:
          goto s_n_llhttp__internal__n_invoke_update_content_length;
        case 3:
          goto s_n_llhttp__internal__n_span_start_llhttp__on_body_1;
        case 4:
          goto s_n_llhttp__internal__n_span_start_llhttp__on_body_2;
        default:
          goto s_n_llhttp__internal__n_invoke_llhttp__on_message_complete;
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_headers_almost_done:
    s_n_llhttp__internal__n_headers_almost_done: {
      if (p == endp) {
        return s_n_llhttp__internal__n_headers_almost_done;
      }
      p++;
      goto s_n_llhttp__internal__n_invoke_test_flags;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_header_value:
    s_n_llhttp__internal__n_span_start_llhttp__on_header_value: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_header_value;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_header_value;
      goto s_n_llhttp__internal__n_span_end_llhttp__on_header_value;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_discard_lws:
    s_n_llhttp__internal__n_header_value_discard_lws: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_discard_lws;
      }
      switch (*p) {
        case 9: {
          p++;
          goto s_n_llhttp__internal__n_header_value_discard_ws;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_header_value_discard_ws;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_load_header_state;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_discard_ws_almost_done:
    s_n_llhttp__internal__n_header_value_discard_ws_almost_done: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_discard_ws_almost_done;
      }
      p++;
      goto s_n_llhttp__internal__n_header_value_discard_lws;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_lws:
    s_n_llhttp__internal__n_header_value_lws: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_lws;
      }
      switch (*p) {
        case 9: {
          goto s_n_llhttp__internal__n_span_start_llhttp__on_header_value_1;
        }
        case ' ': {
          goto s_n_llhttp__internal__n_span_start_llhttp__on_header_value_1;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_load_header_state_2;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_almost_done:
    s_n_llhttp__internal__n_header_value_almost_done: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_almost_done;
      }
      switch (*p) {
        case 10: {
          p++;
          goto s_n_llhttp__internal__n_header_value_lws;
        }
        default: {
          goto s_n_llhttp__internal__n_error_11;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_otherwise:
    s_n_llhttp__internal__n_header_value_otherwise: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_otherwise;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_header_value_1;
        }
        case 13: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_header_value_2;
        }
        default: {
          goto s_n_llhttp__internal__n_error_12;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_connection_token:
    s_n_llhttp__internal__n_header_value_connection_token: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_connection_token;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection_token;
        }
        case 2: {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection;
        }
        default: {
          goto s_n_llhttp__internal__n_header_value_otherwise;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_connection_ws:
    s_n_llhttp__internal__n_header_value_connection_ws: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_connection_ws;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_header_value_otherwise;
        }
        case 13: {
          goto s_n_llhttp__internal__n_header_value_otherwise;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection_ws;
        }
        case ',': {
          p++;
          goto s_n_llhttp__internal__n_invoke_load_header_state_3;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_4;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_connection_1:
    s_n_llhttp__internal__n_header_value_connection_1: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_connection_1;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob3, 4);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_invoke_update_header_state_2;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_value_connection_1;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_header_value_connection_token;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_connection_2:
    s_n_llhttp__internal__n_header_value_connection_2: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_connection_2;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob4, 9);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_invoke_update_header_state_5;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_value_connection_2;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_header_value_connection_token;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_connection_3:
    s_n_llhttp__internal__n_header_value_connection_3: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_connection_3;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob5, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_invoke_update_header_state_6;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_value_connection_3;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_header_value_connection_token;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_connection:
    s_n_llhttp__internal__n_header_value_connection: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_connection;
      }
      switch (((*p) | 0x20)) {
        case 9: {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection;
        }
        case 'c': {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection_1;
        }
        case 'k': {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection_2;
        }
        case 'u': {
          p++;
          goto s_n_llhttp__internal__n_header_value_connection_3;
        }
        default: {
          goto s_n_llhttp__internal__n_header_value_connection_token;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_error_14:
    s_n_llhttp__internal__n_error_14: {
      state->error = 0xb;
      state->reason = "Content-Length overflow";
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_error;
      return s_error;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value:
    s_n_llhttp__internal__n_header_value: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_header_value;
        }
        default: {
          goto s_n_llhttp__internal__n_header_value_otherwise;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_discard_rws:
    s_n_llhttp__internal__n_header_value_discard_rws: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_discard_rws;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_header_value_otherwise;
        }
        case 13: {
          goto s_n_llhttp__internal__n_header_value_otherwise;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_header_value_discard_rws;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_7;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_error_15:
    s_n_llhttp__internal__n_error_15: {
      state->error = 0xb;
      state->reason = "Invalid character in Content-Length";
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_error;
      return s_error;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_content_length_ws:
    s_n_llhttp__internal__n_header_value_content_length_ws: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_content_length_ws;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_invoke_or_flags_15;
        }
        case 13: {
          goto s_n_llhttp__internal__n_invoke_or_flags_15;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_header_value_content_length_ws;
        }
        default: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_header_value_4;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_content_length:
    s_n_llhttp__internal__n_header_value_content_length: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_content_length;
      }
      switch (*p) {
        case '0': {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '1': {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '2': {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '3': {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '4': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '5': {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '6': {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '7': {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '8': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        case '9': {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_mul_add_content_length_1;
        }
        default: {
          goto s_n_llhttp__internal__n_header_value_content_length_ws;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_te_chunked_1:
    s_n_llhttp__internal__n_header_value_te_chunked_1: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_te_chunked_1;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob6, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_invoke_update_header_state_8;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_value_te_chunked_1;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_7;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_te_chunked:
    s_n_llhttp__internal__n_header_value_te_chunked: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_te_chunked;
      }
      switch (((*p) | 0x20)) {
        case 10: {
          goto s_n_llhttp__internal__n_header_value_discard_rws;
        }
        case 13: {
          goto s_n_llhttp__internal__n_header_value_discard_rws;
        }
        case ' ': {
          goto s_n_llhttp__internal__n_header_value_discard_rws;
        }
        case 'c': {
          p++;
          goto s_n_llhttp__internal__n_header_value_te_chunked_1;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_7;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_header_value_1:
    s_n_llhttp__internal__n_span_start_llhttp__on_header_value_1: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_header_value_1;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_header_value;
      goto s_n_llhttp__internal__n_invoke_load_header_state_1;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_value_discard_ws:
    s_n_llhttp__internal__n_header_value_discard_ws: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_value_discard_ws;
      }
      switch (*p) {
        case 9: {
          p++;
          goto s_n_llhttp__internal__n_header_value_discard_ws;
        }
        case 10: {
          p++;
          goto s_n_llhttp__internal__n_header_value_discard_lws;
        }
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_header_value_discard_ws_almost_done;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_header_value_discard_ws;
        }
        default: {
          goto s_n_llhttp__internal__n_span_start_llhttp__on_header_value_1;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_general_otherwise:
    s_n_llhttp__internal__n_header_field_general_otherwise: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_general_otherwise;
      }
      switch (*p) {
        case ':': {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_header_field_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_16;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_general:
    s_n_llhttp__internal__n_header_field_general: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_general;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_header_field_general;
        }
        default: {
          goto s_n_llhttp__internal__n_header_field_general_otherwise;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_colon:
    s_n_llhttp__internal__n_header_field_colon: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_colon;
      }
      switch (*p) {
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_header_field_colon;
        }
        case ':': {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_header_field;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_9;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_3:
    s_n_llhttp__internal__n_header_field_3: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_3;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob2, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_store_header_state;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_field_3;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_4:
    s_n_llhttp__internal__n_header_field_4: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_4;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob7, 10);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_store_header_state;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_field_4;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_2:
    s_n_llhttp__internal__n_header_field_2: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_2;
      }
      switch (((*p) | 0x20)) {
        case 'n': {
          p++;
          goto s_n_llhttp__internal__n_header_field_3;
        }
        case 't': {
          p++;
          goto s_n_llhttp__internal__n_header_field_4;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_1:
    s_n_llhttp__internal__n_header_field_1: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_1;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob1, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_header_field_2;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_field_1;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_5:
    s_n_llhttp__internal__n_header_field_5: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_5;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob8, 15);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_store_header_state;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_field_5;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_6:
    s_n_llhttp__internal__n_header_field_6: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_6;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob9, 16);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_store_header_state;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_field_6;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_7:
    s_n_llhttp__internal__n_header_field_7: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_7;
      }
      match_seq = llparse__match_sequence_to_lower_unsafe(state, p, endp, llparse_blob10, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_store_header_state;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_header_field_7;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field:
    s_n_llhttp__internal__n_header_field: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field;
      }
      switch (((*p) | 0x20)) {
        case 'c': {
          p++;
          goto s_n_llhttp__internal__n_header_field_1;
        }
        case 'p': {
          p++;
          goto s_n_llhttp__internal__n_header_field_5;
        }
        case 't': {
          p++;
          goto s_n_llhttp__internal__n_header_field_6;
        }
        case 'u': {
          p++;
          goto s_n_llhttp__internal__n_header_field_7;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_header_state_10;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_header_field:
    s_n_llhttp__internal__n_span_start_llhttp__on_header_field: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_header_field;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_header_field;
      goto s_n_llhttp__internal__n_header_field;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_header_field_start:
    s_n_llhttp__internal__n_header_field_start: {
      if (p == endp) {
        return s_n_llhttp__internal__n_header_field_start;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_headers_almost_done;
        }
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_headers_almost_done;
        }
        default: {
          goto s_n_llhttp__internal__n_span_start_llhttp__on_header_field;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_skip_to_http09:
    s_n_llhttp__internal__n_url_skip_to_http09: {
      if (p == endp) {
        return s_n_llhttp__internal__n_url_skip_to_http09;
      }
      p++;
      goto s_n_llhttp__internal__n_invoke_update_http_major;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_skip_lf_to_http09:
    s_n_llhttp__internal__n_url_skip_lf_to_http09: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_url_skip_lf_to_http09;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob11, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_invoke_update_http_major;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_url_skip_lf_to_http09;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_17;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_end_1:
    s_n_llhttp__internal__n_req_http_end_1: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_end_1;
      }
      switch (*p) {
        case 10: {
          p++;
          goto s_n_llhttp__internal__n_header_field_start;
        }
        default: {
          goto s_n_llhttp__internal__n_error_18;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_end:
    s_n_llhttp__internal__n_req_http_end: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_end;
      }
      switch (*p) {
        case 10: {
          p++;
          goto s_n_llhttp__internal__n_header_field_start;
        }
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_req_http_end_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_18;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_minor:
    s_n_llhttp__internal__n_req_http_minor: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_minor;
      }
      switch (*p) {
        case '0': {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '1': {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '2': {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '3': {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '4': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '5': {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '6': {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '7': {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '8': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        case '9': {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_store_http_minor;
        }
        default: {
          goto s_n_llhttp__internal__n_error_19;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_dot:
    s_n_llhttp__internal__n_req_http_dot: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_dot;
      }
      switch (*p) {
        case '.': {
          p++;
          goto s_n_llhttp__internal__n_req_http_minor;
        }
        default: {
          goto s_n_llhttp__internal__n_error_20;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_major:
    s_n_llhttp__internal__n_req_http_major: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_major;
      }
      switch (*p) {
        case '0': {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '1': {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '2': {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '3': {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '4': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '5': {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '6': {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '7': {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '8': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        case '9': {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_store_http_major;
        }
        default: {
          goto s_n_llhttp__internal__n_error_21;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_start_1:
    s_n_llhttp__internal__n_req_http_start_1: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_start_1;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob12, 4);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_req_http_major;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_req_http_start_1;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_23;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_start_2:
    s_n_llhttp__internal__n_req_http_start_2: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_start_2;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob13, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_invoke_is_equal_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_req_http_start_2;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_23;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_http_start:
    s_n_llhttp__internal__n_req_http_start: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_http_start;
      }
      switch (*p) {
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_req_http_start;
        }
        case 'H': {
          p++;
          goto s_n_llhttp__internal__n_req_http_start_1;
        }
        case 'I': {
          p++;
          goto s_n_llhttp__internal__n_req_http_start_2;
        }
        default: {
          goto s_n_llhttp__internal__n_error_23;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_skip_to_http:
    s_n_llhttp__internal__n_url_skip_to_http: {
      if (p == endp) {
        return s_n_llhttp__internal__n_url_skip_to_http;
      }
      p++;
      goto s_n_llhttp__internal__n_req_http_start;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_fragment:
    s_n_llhttp__internal__n_url_fragment: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 1, 3, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_url_fragment;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_url_fragment;
        }
        case 2: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_6;
        }
        case 3: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_7;
        }
        case 4: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_8;
        }
        default: {
          goto s_n_llhttp__internal__n_error_24;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_end_stub_query_3:
    s_n_llhttp__internal__n_span_end_stub_query_3: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_end_stub_query_3;
      }
      p++;
      goto s_n_llhttp__internal__n_url_fragment;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_query:
    s_n_llhttp__internal__n_url_query: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 1, 3, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        4, 1, 1, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_url_query;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_url_query;
        }
        case 2: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_9;
        }
        case 3: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_10;
        }
        case 4: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_11;
        }
        case 5: {
          goto s_n_llhttp__internal__n_span_end_stub_query_3;
        }
        default: {
          goto s_n_llhttp__internal__n_error_25;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_query_or_fragment:
    s_n_llhttp__internal__n_url_query_or_fragment: {
      if (p == endp) {
        return s_n_llhttp__internal__n_url_query_or_fragment;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_3;
        }
        case 13: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_4;
        }
        case ' ': {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_5;
        }
        case '#': {
          p++;
          goto s_n_llhttp__internal__n_url_fragment;
        }
        case '?': {
          p++;
          goto s_n_llhttp__internal__n_url_query;
        }
        default: {
          goto s_n_llhttp__internal__n_error_26;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_path:
    s_n_llhttp__internal__n_url_path: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_url_path;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_url_path;
        }
        default: {
          goto s_n_llhttp__internal__n_url_query_or_fragment;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_stub_path_2:
    s_n_llhttp__internal__n_span_start_stub_path_2: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_stub_path_2;
      }
      p++;
      goto s_n_llhttp__internal__n_url_path;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_stub_path:
    s_n_llhttp__internal__n_span_start_stub_path: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_stub_path;
      }
      p++;
      goto s_n_llhttp__internal__n_url_path;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_stub_path_1:
    s_n_llhttp__internal__n_span_start_stub_path_1: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_stub_path_1;
      }
      p++;
      goto s_n_llhttp__internal__n_url_path;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_server_with_at:
    s_n_llhttp__internal__n_url_server_with_at: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        3, 4, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 6,
        7, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4,
        0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_url_server_with_at;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_12;
        }
        case 2: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_13;
        }
        case 3: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_14;
        }
        case 4: {
          p++;
          goto s_n_llhttp__internal__n_url_server;
        }
        case 5: {
          goto s_n_llhttp__internal__n_span_start_stub_path_1;
        }
        case 6: {
          p++;
          goto s_n_llhttp__internal__n_url_query;
        }
        case 7: {
          p++;
          goto s_n_llhttp__internal__n_error_27;
        }
        default: {
          goto s_n_llhttp__internal__n_error_28;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_server:
    s_n_llhttp__internal__n_url_server: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        3, 4, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 6,
        7, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4,
        0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_url_server;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url;
        }
        case 2: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_1;
        }
        case 3: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_url_2;
        }
        case 4: {
          p++;
          goto s_n_llhttp__internal__n_url_server;
        }
        case 5: {
          goto s_n_llhttp__internal__n_span_start_stub_path;
        }
        case 6: {
          p++;
          goto s_n_llhttp__internal__n_url_query;
        }
        case 7: {
          p++;
          goto s_n_llhttp__internal__n_url_server_with_at;
        }
        default: {
          goto s_n_llhttp__internal__n_error_29;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_schema_delim_1:
    s_n_llhttp__internal__n_url_schema_delim_1: {
      if (p == endp) {
        return s_n_llhttp__internal__n_url_schema_delim_1;
      }
      switch (*p) {
        case '/': {
          p++;
          goto s_n_llhttp__internal__n_url_server;
        }
        default: {
          goto s_n_llhttp__internal__n_error_31;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_schema_delim:
    s_n_llhttp__internal__n_url_schema_delim: {
      if (p == endp) {
        return s_n_llhttp__internal__n_url_schema_delim;
      }
      switch (*p) {
        case 10: {
          p++;
          goto s_n_llhttp__internal__n_error_30;
        }
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_error_30;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_error_30;
        }
        case '/': {
          p++;
          goto s_n_llhttp__internal__n_url_schema_delim_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_31;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_end_stub_schema:
    s_n_llhttp__internal__n_span_end_stub_schema: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_end_stub_schema;
      }
      p++;
      goto s_n_llhttp__internal__n_url_schema_delim;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_schema:
    s_n_llhttp__internal__n_url_schema: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,
        0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0,
        0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_url_schema;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_error_30;
        }
        case 2: {
          goto s_n_llhttp__internal__n_span_end_stub_schema;
        }
        case 3: {
          p++;
          goto s_n_llhttp__internal__n_url_schema;
        }
        default: {
          goto s_n_llhttp__internal__n_error_32;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_url_start:
    s_n_llhttp__internal__n_url_start: {
      static uint8_t lookup_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0,
        0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      };
      if (p == endp) {
        return s_n_llhttp__internal__n_url_start;
      }
      switch (lookup_table[(uint8_t) *p]) {
        case 1: {
          p++;
          goto s_n_llhttp__internal__n_error_30;
        }
        case 2: {
          goto s_n_llhttp__internal__n_span_start_stub_path_2;
        }
        case 3: {
          goto s_n_llhttp__internal__n_url_schema;
        }
        default: {
          goto s_n_llhttp__internal__n_error_33;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_url_1:
    s_n_llhttp__internal__n_span_start_llhttp__on_url_1: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_url_1;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_url;
      goto s_n_llhttp__internal__n_url_start;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_url:
    s_n_llhttp__internal__n_span_start_llhttp__on_url: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_url;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_url;
      goto s_n_llhttp__internal__n_url_server;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_spaces_before_url:
    s_n_llhttp__internal__n_req_spaces_before_url: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_spaces_before_url;
      }
      switch (*p) {
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_req_spaces_before_url;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_is_equal_method;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_first_space_before_url:
    s_n_llhttp__internal__n_req_first_space_before_url: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_first_space_before_url;
      }
      switch (*p) {
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_req_spaces_before_url;
        }
        default: {
          goto s_n_llhttp__internal__n_error_34;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_1:
    s_n_llhttp__internal__n_start_req_1: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_1;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob0, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 19;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_1;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_2:
    s_n_llhttp__internal__n_start_req_2: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_2;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob14, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 16;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_2;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_4:
    s_n_llhttp__internal__n_start_req_4: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_4;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob15, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 22;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_4;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_6:
    s_n_llhttp__internal__n_start_req_6: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_6;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob16, 4);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_6;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_7:
    s_n_llhttp__internal__n_start_req_7: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_7;
      }
      switch (*p) {
        case 'Y': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_5:
    s_n_llhttp__internal__n_start_req_5: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_5;
      }
      switch (*p) {
        case 'N': {
          p++;
          goto s_n_llhttp__internal__n_start_req_6;
        }
        case 'P': {
          p++;
          goto s_n_llhttp__internal__n_start_req_7;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_3:
    s_n_llhttp__internal__n_start_req_3: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_3;
      }
      switch (*p) {
        case 'H': {
          p++;
          goto s_n_llhttp__internal__n_start_req_4;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_5;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_8:
    s_n_llhttp__internal__n_start_req_8: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_8;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob17, 5);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_8;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_9:
    s_n_llhttp__internal__n_start_req_9: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_9;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob18, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_9;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_10:
    s_n_llhttp__internal__n_start_req_10: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_10;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob19, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_10;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_12:
    s_n_llhttp__internal__n_start_req_12: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_12;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob20, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 31;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_12;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_13:
    s_n_llhttp__internal__n_start_req_13: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_13;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob21, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_13;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_11:
    s_n_llhttp__internal__n_start_req_11: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_11;
      }
      switch (*p) {
        case 'I': {
          p++;
          goto s_n_llhttp__internal__n_start_req_12;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_13;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_15:
    s_n_llhttp__internal__n_start_req_15: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_15;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob22, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 24;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_15;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_16:
    s_n_llhttp__internal__n_start_req_16: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_16;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob23, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 23;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_16;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_18:
    s_n_llhttp__internal__n_start_req_18: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_18;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob24, 7);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 21;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_18;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_20:
    s_n_llhttp__internal__n_start_req_20: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_20;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob25, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 30;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_20;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_21:
    s_n_llhttp__internal__n_start_req_21: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_21;
      }
      switch (*p) {
        case 'L': {
          p++;
          match = 10;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_19:
    s_n_llhttp__internal__n_start_req_19: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_19;
      }
      switch (*p) {
        case 'A': {
          p++;
          goto s_n_llhttp__internal__n_start_req_20;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_21;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_17:
    s_n_llhttp__internal__n_start_req_17: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_17;
      }
      switch (*p) {
        case 'A': {
          p++;
          goto s_n_llhttp__internal__n_start_req_18;
        }
        case 'C': {
          p++;
          goto s_n_llhttp__internal__n_start_req_19;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_22:
    s_n_llhttp__internal__n_start_req_22: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_22;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob26, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 11;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_22;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_14:
    s_n_llhttp__internal__n_start_req_14: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_14;
      }
      switch (*p) {
        case '-': {
          p++;
          goto s_n_llhttp__internal__n_start_req_15;
        }
        case 'E': {
          p++;
          goto s_n_llhttp__internal__n_start_req_16;
        }
        case 'K': {
          p++;
          goto s_n_llhttp__internal__n_start_req_17;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_22;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_23:
    s_n_llhttp__internal__n_start_req_23: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_23;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob27, 5);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 25;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_23;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_24:
    s_n_llhttp__internal__n_start_req_24: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_24;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob28, 6);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_24;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_26:
    s_n_llhttp__internal__n_start_req_26: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_26;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob29, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 28;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_26;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_27:
    s_n_llhttp__internal__n_start_req_27: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_27;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob30, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_27;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_30:
    s_n_llhttp__internal__n_start_req_30: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_30;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob32, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 12;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_30;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_31:
    s_n_llhttp__internal__n_start_req_31: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_31;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob33, 4);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 13;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_31;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_29:
    s_n_llhttp__internal__n_start_req_29: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_29;
      }
      switch (*p) {
        case 'F': {
          p++;
          goto s_n_llhttp__internal__n_start_req_30;
        }
        case 'P': {
          p++;
          goto s_n_llhttp__internal__n_start_req_31;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_28:
    s_n_llhttp__internal__n_start_req_28: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_28;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob31, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_start_req_29;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_28;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_33:
    s_n_llhttp__internal__n_start_req_33: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_33;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob34, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 29;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_33;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_32:
    s_n_llhttp__internal__n_start_req_32: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_32;
      }
      switch (*p) {
        case 'R': {
          p++;
          goto s_n_llhttp__internal__n_start_req_33;
        }
        case 'T': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_25:
    s_n_llhttp__internal__n_start_req_25: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_25;
      }
      switch (*p) {
        case 'A': {
          p++;
          goto s_n_llhttp__internal__n_start_req_26;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_27;
        }
        case 'R': {
          p++;
          goto s_n_llhttp__internal__n_start_req_28;
        }
        case 'U': {
          p++;
          goto s_n_llhttp__internal__n_start_req_32;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_36:
    s_n_llhttp__internal__n_start_req_36: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_36;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob35, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 17;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_36;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_37:
    s_n_llhttp__internal__n_start_req_37: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_37;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob36, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 20;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_37;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_35:
    s_n_llhttp__internal__n_start_req_35: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_35;
      }
      switch (*p) {
        case 'B': {
          p++;
          goto s_n_llhttp__internal__n_start_req_36;
        }
        case 'P': {
          p++;
          goto s_n_llhttp__internal__n_start_req_37;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_34:
    s_n_llhttp__internal__n_start_req_34: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_34;
      }
      switch (*p) {
        case 'E': {
          p++;
          goto s_n_llhttp__internal__n_start_req_35;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_39:
    s_n_llhttp__internal__n_start_req_39: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_39;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob37, 4);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 14;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_39;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_40:
    s_n_llhttp__internal__n_start_req_40: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_40;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob38, 4);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 33;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_40;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_41:
    s_n_llhttp__internal__n_start_req_41: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_41;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob39, 7);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 26;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_41;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_38:
    s_n_llhttp__internal__n_start_req_38: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_38;
      }
      switch (*p) {
        case 'E': {
          p++;
          goto s_n_llhttp__internal__n_start_req_39;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_40;
        }
        case 'U': {
          p++;
          goto s_n_llhttp__internal__n_start_req_41;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_42:
    s_n_llhttp__internal__n_start_req_42: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_42;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob40, 4);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_42;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_45:
    s_n_llhttp__internal__n_start_req_45: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_45;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob41, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 18;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_45;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_47:
    s_n_llhttp__internal__n_start_req_47: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_47;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob42, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 32;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_47;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_48:
    s_n_llhttp__internal__n_start_req_48: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_48;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob43, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 15;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_48;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_46:
    s_n_llhttp__internal__n_start_req_46: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_46;
      }
      switch (*p) {
        case 'I': {
          p++;
          goto s_n_llhttp__internal__n_start_req_47;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_48;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_49:
    s_n_llhttp__internal__n_start_req_49: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_49;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob44, 8);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 27;
          goto s_n_llhttp__internal__n_invoke_store_method_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_req_49;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_44:
    s_n_llhttp__internal__n_start_req_44: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_44;
      }
      switch (*p) {
        case 'B': {
          p++;
          goto s_n_llhttp__internal__n_start_req_45;
        }
        case 'L': {
          p++;
          goto s_n_llhttp__internal__n_start_req_46;
        }
        case 'S': {
          p++;
          goto s_n_llhttp__internal__n_start_req_49;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_43:
    s_n_llhttp__internal__n_start_req_43: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_43;
      }
      switch (*p) {
        case 'N': {
          p++;
          goto s_n_llhttp__internal__n_start_req_44;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req:
    s_n_llhttp__internal__n_start_req: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req;
      }
      switch (*p) {
        case 'A': {
          p++;
          goto s_n_llhttp__internal__n_start_req_1;
        }
        case 'B': {
          p++;
          goto s_n_llhttp__internal__n_start_req_2;
        }
        case 'C': {
          p++;
          goto s_n_llhttp__internal__n_start_req_3;
        }
        case 'D': {
          p++;
          goto s_n_llhttp__internal__n_start_req_8;
        }
        case 'G': {
          p++;
          goto s_n_llhttp__internal__n_start_req_9;
        }
        case 'H': {
          p++;
          goto s_n_llhttp__internal__n_start_req_10;
        }
        case 'L': {
          p++;
          goto s_n_llhttp__internal__n_start_req_11;
        }
        case 'M': {
          p++;
          goto s_n_llhttp__internal__n_start_req_14;
        }
        case 'N': {
          p++;
          goto s_n_llhttp__internal__n_start_req_23;
        }
        case 'O': {
          p++;
          goto s_n_llhttp__internal__n_start_req_24;
        }
        case 'P': {
          p++;
          goto s_n_llhttp__internal__n_start_req_25;
        }
        case 'R': {
          p++;
          goto s_n_llhttp__internal__n_start_req_34;
        }
        case 'S': {
          p++;
          goto s_n_llhttp__internal__n_start_req_38;
        }
        case 'T': {
          p++;
          goto s_n_llhttp__internal__n_start_req_42;
        }
        case 'U': {
          p++;
          goto s_n_llhttp__internal__n_start_req_43;
        }
        default: {
          goto s_n_llhttp__internal__n_error_42;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_line_almost_done:
    s_n_llhttp__internal__n_res_line_almost_done: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_line_almost_done;
      }
      p++;
      goto s_n_llhttp__internal__n_header_field_start;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_status:
    s_n_llhttp__internal__n_res_status: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_status;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_status;
        }
        case 13: {
          goto s_n_llhttp__internal__n_span_end_llhttp__on_status_1;
        }
        default: {
          p++;
          goto s_n_llhttp__internal__n_res_status;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_span_start_llhttp__on_status:
    s_n_llhttp__internal__n_span_start_llhttp__on_status: {
      if (p == endp) {
        return s_n_llhttp__internal__n_span_start_llhttp__on_status;
      }
      state->_span_pos0 = (void*) p;
      state->_span_cb0 = llhttp__on_status;
      goto s_n_llhttp__internal__n_res_status;
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_status_start:
    s_n_llhttp__internal__n_res_status_start: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_status_start;
      }
      switch (*p) {
        case 10: {
          p++;
          goto s_n_llhttp__internal__n_header_field_start;
        }
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_res_line_almost_done;
        }
        default: {
          goto s_n_llhttp__internal__n_span_start_llhttp__on_status;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_status_code_otherwise:
    s_n_llhttp__internal__n_res_status_code_otherwise: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_status_code_otherwise;
      }
      switch (*p) {
        case 10: {
          goto s_n_llhttp__internal__n_res_status_start;
        }
        case 13: {
          goto s_n_llhttp__internal__n_res_status_start;
        }
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_res_status_start;
        }
        default: {
          goto s_n_llhttp__internal__n_error_36;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_status_code:
    s_n_llhttp__internal__n_res_status_code: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_status_code;
      }
      switch (*p) {
        case '0': {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '1': {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '2': {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '3': {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '4': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '5': {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '6': {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '7': {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '8': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        case '9': {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_mul_add_status_code;
        }
        default: {
          goto s_n_llhttp__internal__n_res_status_code_otherwise;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_http_end:
    s_n_llhttp__internal__n_res_http_end: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_http_end;
      }
      switch (*p) {
        case ' ': {
          p++;
          goto s_n_llhttp__internal__n_invoke_update_status_code;
        }
        default: {
          goto s_n_llhttp__internal__n_error_37;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_http_minor:
    s_n_llhttp__internal__n_res_http_minor: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_http_minor;
      }
      switch (*p) {
        case '0': {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '1': {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '2': {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '3': {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '4': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '5': {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '6': {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '7': {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '8': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        case '9': {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_store_http_minor_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_38;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_http_dot:
    s_n_llhttp__internal__n_res_http_dot: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_http_dot;
      }
      switch (*p) {
        case '.': {
          p++;
          goto s_n_llhttp__internal__n_res_http_minor;
        }
        default: {
          goto s_n_llhttp__internal__n_error_39;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_res_http_major:
    s_n_llhttp__internal__n_res_http_major: {
      if (p == endp) {
        return s_n_llhttp__internal__n_res_http_major;
      }
      switch (*p) {
        case '0': {
          p++;
          match = 0;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '1': {
          p++;
          match = 1;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '2': {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '3': {
          p++;
          match = 3;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '4': {
          p++;
          match = 4;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '5': {
          p++;
          match = 5;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '6': {
          p++;
          match = 6;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '7': {
          p++;
          match = 7;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '8': {
          p++;
          match = 8;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        case '9': {
          p++;
          match = 9;
          goto s_n_llhttp__internal__n_invoke_store_http_major_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_40;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_res:
    s_n_llhttp__internal__n_start_res: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_start_res;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob45, 5);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_res_http_major;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_start_res;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_43;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_or_res_method_2:
    s_n_llhttp__internal__n_req_or_res_method_2: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_req_or_res_method_2;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob46, 2);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          match = 2;
          goto s_n_llhttp__internal__n_invoke_store_method;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_req_or_res_method_2;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_41;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_or_res_method_3:
    s_n_llhttp__internal__n_req_or_res_method_3: {
      llparse_match_t match_seq;
      
      if (p == endp) {
        return s_n_llhttp__internal__n_req_or_res_method_3;
      }
      match_seq = llparse__match_sequence_id(state, p, endp, llparse_blob47, 3);
      p = match_seq.current;
      switch (match_seq.status) {
        case kMatchComplete: {
          p++;
          goto s_n_llhttp__internal__n_invoke_update_type_1;
        }
        case kMatchPause: {
          return s_n_llhttp__internal__n_req_or_res_method_3;
        }
        case kMatchMismatch: {
          goto s_n_llhttp__internal__n_error_41;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_or_res_method_1:
    s_n_llhttp__internal__n_req_or_res_method_1: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_or_res_method_1;
      }
      switch (*p) {
        case 'E': {
          p++;
          goto s_n_llhttp__internal__n_req_or_res_method_2;
        }
        case 'T': {
          p++;
          goto s_n_llhttp__internal__n_req_or_res_method_3;
        }
        default: {
          goto s_n_llhttp__internal__n_error_41;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_req_or_res_method:
    s_n_llhttp__internal__n_req_or_res_method: {
      if (p == endp) {
        return s_n_llhttp__internal__n_req_or_res_method;
      }
      switch (*p) {
        case 'H': {
          p++;
          goto s_n_llhttp__internal__n_req_or_res_method_1;
        }
        default: {
          goto s_n_llhttp__internal__n_error_41;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start_req_or_res:
    s_n_llhttp__internal__n_start_req_or_res: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start_req_or_res;
      }
      switch (*p) {
        case 'H': {
          goto s_n_llhttp__internal__n_req_or_res_method;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_type_2;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_invoke_load_type:
    s_n_llhttp__internal__n_invoke_load_type: {
      switch (llhttp__internal__c_load_type(state, p, endp)) {
        case 1:
          goto s_n_llhttp__internal__n_start_req;
        case 2:
          goto s_n_llhttp__internal__n_start_res;
        default:
          goto s_n_llhttp__internal__n_start_req_or_res;
      }
      /* UNREACHABLE */;
      abort();
    }
    case s_n_llhttp__internal__n_start:
    s_n_llhttp__internal__n_start: {
      if (p == endp) {
        return s_n_llhttp__internal__n_start;
      }
      switch (*p) {
        case 10: {
          p++;
          goto s_n_llhttp__internal__n_start;
        }
        case 13: {
          p++;
          goto s_n_llhttp__internal__n_start;
        }
        default: {
          goto s_n_llhttp__internal__n_invoke_update_finish;
        }
      }
      /* UNREACHABLE */;
      abort();
    }
    default:
      /* UNREACHABLE */
      abort();
  }
  s_n_llhttp__internal__n_error_30: {
    state->error = 0x7;
    state->reason = "Invalid characters in url";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause_5: {
    state->error = 0x14;
    state->reason = "on_message_complete pause";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_is_equal_upgrade;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_8: {
    state->error = 0x11;
    state->reason = "`on_message_complete` callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause_7: {
    state->error = 0x14;
    state->reason = "on_chunk_complete pause";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_2;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_10: {
    state->error = 0x13;
    state->reason = "`on_chunk_complete` callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__on_chunk_complete_1: {
    switch (llhttp__on_chunk_complete(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_2;
      case 20:
        goto s_n_llhttp__internal__n_pause_7;
      default:
        goto s_n_llhttp__internal__n_error_10;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_9: {
    state->error = 0x4;
    state->reason = "Content-Length can't be present with chunked encoding";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause_2: {
    state->error = 0x14;
    state->reason = "on_message_complete pause";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_pause_1;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_3: {
    state->error = 0x11;
    state->reason = "`on_message_complete` callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_1: {
    switch (llhttp__on_message_complete(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_pause_1;
      case 20:
        goto s_n_llhttp__internal__n_pause_2;
      default:
        goto s_n_llhttp__internal__n_error_3;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_4: {
    state->error = 0xc;
    state->reason = "Chunk size overflow";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_mul_add_content_length: {
    switch (llhttp__internal__c_mul_add_content_length(state, p, endp, match)) {
      case 1:
        goto s_n_llhttp__internal__n_error_4;
      default:
        goto s_n_llhttp__internal__n_chunk_size;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause_3: {
    state->error = 0x14;
    state->reason = "on_chunk_complete pause";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_update_content_length;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_6: {
    state->error = 0x13;
    state->reason = "`on_chunk_complete` callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__on_chunk_complete: {
    switch (llhttp__on_chunk_complete(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_invoke_update_content_length;
      case 20:
        goto s_n_llhttp__internal__n_pause_3;
      default:
        goto s_n_llhttp__internal__n_error_6;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_body: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_body(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_chunk_data_almost_done;
      return s_error;
    }
    goto s_n_llhttp__internal__n_chunk_data_almost_done;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags: {
    switch (llhttp__internal__c_or_flags(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_field_start;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause_4: {
    state->error = 0x14;
    state->reason = "on_chunk_header pause";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_is_equal_content_length;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_5: {
    state->error = 0x12;
    state->reason = "`on_chunk_header` callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__on_chunk_header: {
    switch (llhttp__on_chunk_header(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_invoke_is_equal_content_length;
      case 20:
        goto s_n_llhttp__internal__n_pause_4;
      default:
        goto s_n_llhttp__internal__n_error_5;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_7: {
    state->error = 0xc;
    state->reason = "Invalid character in chunk size";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_body_1: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_body(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_2;
      return s_error;
    }
    goto s_n_llhttp__internal__n_invoke_llhttp__on_message_complete_2;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_finish_1: {
    switch (llhttp__internal__c_update_finish_1(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_eof;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause: {
    state->error = 0x14;
    state->reason = "on_message_complete pause";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_llhttp__after_message_complete;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_2: {
    state->error = 0x11;
    state->reason = "`on_message_complete` callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__on_message_complete: {
    switch (llhttp__on_message_complete(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_invoke_llhttp__after_message_complete;
      case 20:
        goto s_n_llhttp__internal__n_pause;
      default:
        goto s_n_llhttp__internal__n_error_2;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_1: {
    switch (llhttp__internal__c_or_flags_1(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_llhttp__after_headers_complete;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_2: {
    switch (llhttp__internal__c_or_flags_1(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_llhttp__after_headers_complete;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_upgrade: {
    switch (llhttp__internal__c_update_upgrade(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_or_flags_2;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause_6: {
    state->error = 0x14;
    state->reason = "Paused by on_headers_complete";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_llhttp__after_headers_complete;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_1: {
    state->error = 0x10;
    state->reason = "User callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__on_headers_complete: {
    switch (llhttp__on_headers_complete(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_invoke_llhttp__after_headers_complete;
      case 1:
        goto s_n_llhttp__internal__n_invoke_or_flags_1;
      case 2:
        goto s_n_llhttp__internal__n_invoke_update_upgrade;
      case 20:
        goto s_n_llhttp__internal__n_pause_6;
      default:
        goto s_n_llhttp__internal__n_error_1;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__before_headers_complete: {
    switch (llhttp__before_headers_complete(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_llhttp__on_headers_complete;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_test_flags_1: {
    switch (llhttp__internal__c_test_flags_1(state, p, endp)) {
      case 1:
        goto s_n_llhttp__internal__n_error_9;
      default:
        goto s_n_llhttp__internal__n_invoke_llhttp__before_headers_complete;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_test_flags: {
    switch (llhttp__internal__c_test_flags(state, p, endp)) {
      case 1:
        goto s_n_llhttp__internal__n_invoke_llhttp__on_chunk_complete_1;
      default:
        goto s_n_llhttp__internal__n_invoke_test_flags_1;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_header_value: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_header_value(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_header_field_start;
      return s_error;
    }
    goto s_n_llhttp__internal__n_header_field_start;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state: {
    switch (llhttp__internal__c_update_header_state(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_span_start_llhttp__on_header_value;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_3: {
    switch (llhttp__internal__c_or_flags_3(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_4: {
    switch (llhttp__internal__c_or_flags_4(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_5: {
    switch (llhttp__internal__c_or_flags_5(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_6: {
    switch (llhttp__internal__c_or_flags_6(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_span_start_llhttp__on_header_value;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_load_header_state: {
    switch (llhttp__internal__c_load_header_state(state, p, endp)) {
      case 5:
        goto s_n_llhttp__internal__n_invoke_or_flags_3;
      case 6:
        goto s_n_llhttp__internal__n_invoke_or_flags_4;
      case 7:
        goto s_n_llhttp__internal__n_invoke_or_flags_5;
      case 8:
        goto s_n_llhttp__internal__n_invoke_or_flags_6;
      default:
        goto s_n_llhttp__internal__n_span_start_llhttp__on_header_value;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_1: {
    switch (llhttp__internal__c_update_header_state(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_field_start;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_7: {
    switch (llhttp__internal__c_or_flags_3(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state_1;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_8: {
    switch (llhttp__internal__c_or_flags_4(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state_1;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_9: {
    switch (llhttp__internal__c_or_flags_5(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state_1;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_10: {
    switch (llhttp__internal__c_or_flags_6(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_field_start;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_load_header_state_2: {
    switch (llhttp__internal__c_load_header_state(state, p, endp)) {
      case 5:
        goto s_n_llhttp__internal__n_invoke_or_flags_7;
      case 6:
        goto s_n_llhttp__internal__n_invoke_or_flags_8;
      case 7:
        goto s_n_llhttp__internal__n_invoke_or_flags_9;
      case 8:
        goto s_n_llhttp__internal__n_invoke_or_flags_10;
      default:
        goto s_n_llhttp__internal__n_header_field_start;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_11: {
    state->error = 0x3;
    state->reason = "Missing expected LF after header value";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_header_value_1: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_header_value(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_header_value_almost_done;
      return s_error;
    }
    goto s_n_llhttp__internal__n_header_value_almost_done;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_header_value_2: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_header_value(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) (p + 1);
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_header_value_almost_done;
      return s_error;
    }
    p++;
    goto s_n_llhttp__internal__n_header_value_almost_done;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_12: {
    state->error = 0xa;
    state->reason = "Invalid header value char";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_3: {
    switch (llhttp__internal__c_update_header_state(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_connection;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_11: {
    switch (llhttp__internal__c_or_flags_3(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state_3;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_12: {
    switch (llhttp__internal__c_or_flags_4(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state_3;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_13: {
    switch (llhttp__internal__c_or_flags_5(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state_3;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_14: {
    switch (llhttp__internal__c_or_flags_6(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_connection;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_load_header_state_3: {
    switch (llhttp__internal__c_load_header_state(state, p, endp)) {
      case 5:
        goto s_n_llhttp__internal__n_invoke_or_flags_11;
      case 6:
        goto s_n_llhttp__internal__n_invoke_or_flags_12;
      case 7:
        goto s_n_llhttp__internal__n_invoke_or_flags_13;
      case 8:
        goto s_n_llhttp__internal__n_invoke_or_flags_14;
      default:
        goto s_n_llhttp__internal__n_header_value_connection;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_4: {
    switch (llhttp__internal__c_update_header_state_4(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_connection_token;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_2: {
    switch (llhttp__internal__c_update_header_state_2(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_connection_ws;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_5: {
    switch (llhttp__internal__c_update_header_state_5(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_connection_ws;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_6: {
    switch (llhttp__internal__c_update_header_state_6(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_connection_ws;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_header_value_3: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_header_value(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_error_14;
      return s_error;
    }
    goto s_n_llhttp__internal__n_error_14;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_mul_add_content_length_1: {
    switch (llhttp__internal__c_mul_add_content_length_1(state, p, endp, match)) {
      case 1:
        goto s_n_llhttp__internal__n_span_end_llhttp__on_header_value_3;
      default:
        goto s_n_llhttp__internal__n_header_value_content_length;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_7: {
    switch (llhttp__internal__c_update_header_state_4(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_15: {
    switch (llhttp__internal__c_or_flags_15(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_discard_rws;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_header_value_4: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_header_value(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_error_15;
      return s_error;
    }
    goto s_n_llhttp__internal__n_error_15;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_13: {
    state->error = 0x4;
    state->reason = "Duplicate Content-Length";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_test_flags_2: {
    switch (llhttp__internal__c_test_flags_2(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_header_value_content_length;
      default:
        goto s_n_llhttp__internal__n_error_13;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_8: {
    switch (llhttp__internal__c_update_header_state_8(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_value_discard_rws;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_or_flags_16: {
    switch (llhttp__internal__c_or_flags_16(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_header_state_7;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_load_header_state_1: {
    switch (llhttp__internal__c_load_header_state(state, p, endp)) {
      case 1:
        goto s_n_llhttp__internal__n_header_value_connection;
      case 2:
        goto s_n_llhttp__internal__n_invoke_test_flags_2;
      case 3:
        goto s_n_llhttp__internal__n_header_value_te_chunked;
      case 4:
        goto s_n_llhttp__internal__n_invoke_or_flags_16;
      default:
        goto s_n_llhttp__internal__n_header_value;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_header_field: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_header_field(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) (p + 1);
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_header_value_discard_ws;
      return s_error;
    }
    p++;
    goto s_n_llhttp__internal__n_header_value_discard_ws;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_header_field_1: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_header_field(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) (p + 1);
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_header_value_discard_ws;
      return s_error;
    }
    p++;
    goto s_n_llhttp__internal__n_header_value_discard_ws;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_16: {
    state->error = 0xa;
    state->reason = "Invalid header token";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_9: {
    switch (llhttp__internal__c_update_header_state_4(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_field_general;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_store_header_state: {
    switch (llhttp__internal__c_store_header_state(state, p, endp, match)) {
      default:
        goto s_n_llhttp__internal__n_header_field_colon;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_header_state_10: {
    switch (llhttp__internal__c_update_header_state_4(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_field_general;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_http_minor: {
    switch (llhttp__internal__c_update_http_minor(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_header_field_start;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_http_major: {
    switch (llhttp__internal__c_update_http_major(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_http_minor;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_3: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_17: {
    state->error = 0x7;
    state->reason = "Expected CRLF";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_4: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_lf_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_lf_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_18: {
    state->error = 0x9;
    state->reason = "Expected CRLF after version";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_store_http_minor: {
    switch (llhttp__internal__c_store_http_minor(state, p, endp, match)) {
      default:
        goto s_n_llhttp__internal__n_req_http_end;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_19: {
    state->error = 0x9;
    state->reason = "Invalid minor version";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_20: {
    state->error = 0x9;
    state->reason = "Expected dot";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_store_http_major: {
    switch (llhttp__internal__c_store_http_major(state, p, endp, match)) {
      default:
        goto s_n_llhttp__internal__n_req_http_dot;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_21: {
    state->error = 0x9;
    state->reason = "Invalid major version";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_23: {
    state->error = 0x8;
    state->reason = "Expected HTTP/";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_22: {
    state->error = 0x8;
    state->reason = "Expected SOURCE method for ICE/x.x request";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_is_equal_method_1: {
    switch (llhttp__internal__c_is_equal_method_1(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_error_22;
      default:
        goto s_n_llhttp__internal__n_req_http_major;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_5: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_6: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_7: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_lf_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_lf_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_8: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_24: {
    state->error = 0x7;
    state->reason = "Invalid char in url fragment start";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_9: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_10: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_lf_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_lf_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_11: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_25: {
    state->error = 0x7;
    state->reason = "Invalid char in url query";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_26: {
    state->error = 0x7;
    state->reason = "Invalid char in url path";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_1: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_lf_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_lf_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_2: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_12: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_13: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_lf_to_http09;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_lf_to_http09;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_url_14: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_url(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) p;
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_url_skip_to_http;
      return s_error;
    }
    goto s_n_llhttp__internal__n_url_skip_to_http;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_27: {
    state->error = 0x7;
    state->reason = "Double @ in url";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_28: {
    state->error = 0x7;
    state->reason = "Unexpected char in url server";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_29: {
    state->error = 0x7;
    state->reason = "Unexpected char in url server";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_31: {
    state->error = 0x7;
    state->reason = "Unexpected char in url schema";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_32: {
    state->error = 0x7;
    state->reason = "Unexpected char in url schema";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_33: {
    state->error = 0x7;
    state->reason = "Unexpected start char in url";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_is_equal_method: {
    switch (llhttp__internal__c_is_equal_method(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_span_start_llhttp__on_url_1;
      default:
        goto s_n_llhttp__internal__n_span_start_llhttp__on_url;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_34: {
    state->error = 0x6;
    state->reason = "Expected space after method";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_store_method_1: {
    switch (llhttp__internal__c_store_method(state, p, endp, match)) {
      default:
        goto s_n_llhttp__internal__n_req_first_space_before_url;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_42: {
    state->error = 0x6;
    state->reason = "Invalid method encountered";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_35: {
    state->error = 0xd;
    state->reason = "Response overflow";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_mul_add_status_code: {
    switch (llhttp__internal__c_mul_add_status_code(state, p, endp, match)) {
      case 1:
        goto s_n_llhttp__internal__n_error_35;
      default:
        goto s_n_llhttp__internal__n_res_status_code;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_status: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_status(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) (p + 1);
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_header_field_start;
      return s_error;
    }
    p++;
    goto s_n_llhttp__internal__n_header_field_start;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_span_end_llhttp__on_status_1: {
    const unsigned char* start;
    int err;
    
    start = state->_span_pos0;
    state->_span_pos0 = NULL;
    err = llhttp__on_status(state, start, p);
    if (err != 0) {
      state->error = err;
      state->error_pos = (const char*) (p + 1);
      state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_res_line_almost_done;
      return s_error;
    }
    p++;
    goto s_n_llhttp__internal__n_res_line_almost_done;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_36: {
    state->error = 0xd;
    state->reason = "Invalid response status";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_status_code: {
    switch (llhttp__internal__c_update_status_code(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_res_status_code;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_37: {
    state->error = 0x9;
    state->reason = "Expected space after version";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_store_http_minor_1: {
    switch (llhttp__internal__c_store_http_minor(state, p, endp, match)) {
      default:
        goto s_n_llhttp__internal__n_res_http_end;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_38: {
    state->error = 0x9;
    state->reason = "Invalid minor version";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_39: {
    state->error = 0x9;
    state->reason = "Expected dot";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_store_http_major_1: {
    switch (llhttp__internal__c_store_http_major(state, p, endp, match)) {
      default:
        goto s_n_llhttp__internal__n_res_http_dot;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_40: {
    state->error = 0x9;
    state->reason = "Invalid major version";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_43: {
    state->error = 0x8;
    state->reason = "Expected HTTP/";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_type: {
    switch (llhttp__internal__c_update_type(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_req_first_space_before_url;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_store_method: {
    switch (llhttp__internal__c_store_method(state, p, endp, match)) {
      default:
        goto s_n_llhttp__internal__n_invoke_update_type;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error_41: {
    state->error = 0x8;
    state->reason = "Invalid word encountered";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_type_1: {
    switch (llhttp__internal__c_update_type_1(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_res_http_major;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_type_2: {
    switch (llhttp__internal__c_update_type(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_start_req;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_pause_8: {
    state->error = 0x14;
    state->reason = "on_message_begin pause";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_n_llhttp__internal__n_invoke_load_type;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_error: {
    state->error = 0xf;
    state->reason = "`on_message_begin` callback error";
    state->error_pos = (const char*) p;
    state->_current = (void*) (intptr_t) s_error;
    return s_error;
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_llhttp__on_message_begin: {
    switch (llhttp__on_message_begin(state, p, endp)) {
      case 0:
        goto s_n_llhttp__internal__n_invoke_load_type;
      case 20:
        goto s_n_llhttp__internal__n_pause_8;
      default:
        goto s_n_llhttp__internal__n_error;
    }
    /* UNREACHABLE */;
    abort();
  }
  s_n_llhttp__internal__n_invoke_update_finish: {
    switch (llhttp__internal__c_update_finish(state, p, endp)) {
      default:
        goto s_n_llhttp__internal__n_invoke_llhttp__on_message_begin;
    }
    /* UNREACHABLE */;
    abort();
  }
}

int llhttp__internal_execute(llhttp__internal_t* state, const char* p, const char* endp) {
  llparse_state_t next;

  /* check lingering errors */
  if (state->error != 0) {
    return state->error;
  }

  /* restart spans */
  if (state->_span_pos0 != NULL) {
    state->_span_pos0 = (void*) p;
  }
  
  next = llhttp__internal__run(state, (const unsigned char*) p, (const unsigned char*) endp);
  if (next == s_error) {
    return state->error;
  }
  state->_current = (void*) (intptr_t) next;

  /* execute spans */
  if (state->_span_pos0 != NULL) {
    int error;
  
    error = ((llhttp__internal__span_cb) state->_span_cb0)(state, state->_span_pos0, (const char*) endp);
    if (error != 0) {
      state->error = error;
      state->error_pos = endp;
      return error;
    }
  }
  
  return 0;
}