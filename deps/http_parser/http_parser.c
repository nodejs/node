#line 1 "http_parser.rl"
/* Copyright (c) 2008, 2009 Ryan Dahl (ry@tinyclouds.org)
 * Based on Zed Shaw's Mongrel, copyright (c) Zed A. Shaw
 *
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 */
#include "http_parser.h"
#include <limits.h>
#include <assert.h>

static int unhex[] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     };
#define TRUE 1
#define FALSE 0
#define MIN(a,b) (a < b ? a : b)
#define NULL (void*)(0)

#define MAX_FIELD_SIZE 80*1024

#define REMAINING (unsigned long)(pe - p)
#define CALLBACK(FOR)                                                \
do {                                                                 \
  if (parser->FOR##_mark) {                                          \
    parser->FOR##_size += p - parser->FOR##_mark;                    \
    if (parser->FOR##_size > MAX_FIELD_SIZE) {                       \
      parser->error = TRUE;                                          \
      return 0;                                                      \
    }                                                                \
    if (parser->on_##FOR) {                                          \
      callback_return_value = parser->on_##FOR(parser,               \
        parser->FOR##_mark,                                          \
        p - parser->FOR##_mark);                                     \
    }                                                                \
  }                                                                  \
} while(0)

#define RESET_PARSER(parser)                                         \
    parser->chunk_size = 0;                                          \
    parser->eating = 0;                                              \
    parser->header_field_mark = NULL;                                \
    parser->header_value_mark = NULL;                                \
    parser->query_string_mark = NULL;                                \
    parser->path_mark = NULL;                                        \
    parser->uri_mark = NULL;                                         \
    parser->fragment_mark = NULL;                                    \
    parser->status_code = 0;                                         \
    parser->method = 0;                                              \
    parser->transfer_encoding = HTTP_IDENTITY;                       \
    parser->version_major = 0;                                       \
    parser->version_minor = 0;                                       \
    parser->keep_alive = -1;                                         \
    parser->content_length = 0;                                      \
    parser->body_read = 0; 

#define END_REQUEST                                                  \
do {                                                                 \
    if (parser->on_message_complete) {                               \
      callback_return_value =                                        \
        parser->on_message_complete(parser);                         \
    }                                                                \
    RESET_PARSER(parser);                                            \
} while (0)

#define SKIP_BODY(nskip)                                             \
do {                                                                 \
  tmp = (nskip);                                                     \
  if (parser->on_body && tmp > 0) {                                  \
    callback_return_value = parser->on_body(parser, p, tmp);         \
  }                                                                  \
  if (callback_return_value == 0) {                                  \
    p += tmp;                                                        \
    parser->body_read += tmp;                                        \
    parser->chunk_size -= tmp;                                       \
    if (0 == parser->chunk_size) {                                   \
      parser->eating = FALSE;                                        \
      if (parser->transfer_encoding == HTTP_IDENTITY) {              \
        END_REQUEST;                                                 \
      }                                                              \
    } else {                                                         \
      parser->eating = TRUE;                                         \
    }                                                                \
  }                                                                  \
} while (0)

#line 413 "http_parser.rl"



#line 116 "http_parser.c"
static const int http_parser_start = 1;
static const int http_parser_first_final = 266;
static const int http_parser_error = 0;

static const int http_parser_en_ChunkedBody = 2;
static const int http_parser_en_ChunkedBody_chunk_chunk_end = 12;
static const int http_parser_en_Requests = 268;
static const int http_parser_en_Responses = 269;
static const int http_parser_en_main = 1;

#line 416 "http_parser.rl"

void
http_parser_init (http_parser *parser, enum http_parser_type type) 
{
  int cs = 0;
  
#line 134 "http_parser.c"
	{
	cs = http_parser_start;
	}
#line 422 "http_parser.rl"
  parser->cs = cs;
  parser->type = type;
  parser->error = 0;

  parser->on_message_begin = NULL;
  parser->on_path = NULL;
  parser->on_query_string = NULL;
  parser->on_uri = NULL;
  parser->on_fragment = NULL;
  parser->on_header_field = NULL;
  parser->on_header_value = NULL;
  parser->on_headers_complete = NULL;
  parser->on_body = NULL;
  parser->on_message_complete = NULL;

  RESET_PARSER(parser);
}

/** exec **/
size_t
http_parser_execute (http_parser *parser, const char *buffer, size_t len)
{
  size_t tmp; // REMOVE ME this is extremely hacky
  int callback_return_value = 0;
  const char *p, *pe;
  int cs = parser->cs;

  p = buffer;
  pe = buffer+len;

  if (0 < parser->chunk_size && parser->eating) {
    /* eat body */
    SKIP_BODY(MIN(len, parser->chunk_size));
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
  }

  if (parser->header_field_mark)   parser->header_field_mark   = buffer;
  if (parser->header_value_mark)   parser->header_value_mark   = buffer;
  if (parser->fragment_mark)       parser->fragment_mark       = buffer;
  if (parser->query_string_mark)   parser->query_string_mark   = buffer;
  if (parser->path_mark)           parser->path_mark           = buffer;
  if (parser->uri_mark)            parser->uri_mark            = buffer;

  
#line 186 "http_parser.c"
	{
	if ( p == pe )
		goto _test_eof;
	goto _resume;

_again:
	switch ( cs ) {
		case 1: goto st1;
		case 266: goto st266;
		case 0: goto st0;
		case 2: goto st2;
		case 3: goto st3;
		case 4: goto st4;
		case 5: goto st5;
		case 6: goto st6;
		case 267: goto st267;
		case 7: goto st7;
		case 8: goto st8;
		case 9: goto st9;
		case 10: goto st10;
		case 11: goto st11;
		case 12: goto st12;
		case 13: goto st13;
		case 14: goto st14;
		case 15: goto st15;
		case 16: goto st16;
		case 17: goto st17;
		case 18: goto st18;
		case 19: goto st19;
		case 268: goto st268;
		case 20: goto st20;
		case 21: goto st21;
		case 22: goto st22;
		case 23: goto st23;
		case 24: goto st24;
		case 25: goto st25;
		case 26: goto st26;
		case 27: goto st27;
		case 28: goto st28;
		case 29: goto st29;
		case 30: goto st30;
		case 31: goto st31;
		case 32: goto st32;
		case 33: goto st33;
		case 34: goto st34;
		case 35: goto st35;
		case 36: goto st36;
		case 37: goto st37;
		case 38: goto st38;
		case 39: goto st39;
		case 40: goto st40;
		case 41: goto st41;
		case 42: goto st42;
		case 43: goto st43;
		case 44: goto st44;
		case 45: goto st45;
		case 46: goto st46;
		case 47: goto st47;
		case 48: goto st48;
		case 49: goto st49;
		case 50: goto st50;
		case 51: goto st51;
		case 52: goto st52;
		case 53: goto st53;
		case 54: goto st54;
		case 55: goto st55;
		case 56: goto st56;
		case 57: goto st57;
		case 58: goto st58;
		case 59: goto st59;
		case 60: goto st60;
		case 61: goto st61;
		case 62: goto st62;
		case 63: goto st63;
		case 64: goto st64;
		case 65: goto st65;
		case 66: goto st66;
		case 67: goto st67;
		case 68: goto st68;
		case 69: goto st69;
		case 70: goto st70;
		case 71: goto st71;
		case 72: goto st72;
		case 73: goto st73;
		case 74: goto st74;
		case 75: goto st75;
		case 76: goto st76;
		case 77: goto st77;
		case 78: goto st78;
		case 79: goto st79;
		case 80: goto st80;
		case 81: goto st81;
		case 82: goto st82;
		case 83: goto st83;
		case 84: goto st84;
		case 85: goto st85;
		case 86: goto st86;
		case 87: goto st87;
		case 88: goto st88;
		case 89: goto st89;
		case 90: goto st90;
		case 91: goto st91;
		case 92: goto st92;
		case 93: goto st93;
		case 94: goto st94;
		case 95: goto st95;
		case 96: goto st96;
		case 97: goto st97;
		case 98: goto st98;
		case 99: goto st99;
		case 100: goto st100;
		case 101: goto st101;
		case 102: goto st102;
		case 103: goto st103;
		case 104: goto st104;
		case 105: goto st105;
		case 106: goto st106;
		case 107: goto st107;
		case 108: goto st108;
		case 109: goto st109;
		case 110: goto st110;
		case 111: goto st111;
		case 112: goto st112;
		case 113: goto st113;
		case 114: goto st114;
		case 115: goto st115;
		case 116: goto st116;
		case 117: goto st117;
		case 118: goto st118;
		case 119: goto st119;
		case 120: goto st120;
		case 121: goto st121;
		case 122: goto st122;
		case 123: goto st123;
		case 124: goto st124;
		case 125: goto st125;
		case 126: goto st126;
		case 127: goto st127;
		case 128: goto st128;
		case 129: goto st129;
		case 130: goto st130;
		case 131: goto st131;
		case 132: goto st132;
		case 133: goto st133;
		case 134: goto st134;
		case 135: goto st135;
		case 136: goto st136;
		case 137: goto st137;
		case 138: goto st138;
		case 139: goto st139;
		case 140: goto st140;
		case 141: goto st141;
		case 142: goto st142;
		case 143: goto st143;
		case 144: goto st144;
		case 145: goto st145;
		case 146: goto st146;
		case 147: goto st147;
		case 148: goto st148;
		case 149: goto st149;
		case 150: goto st150;
		case 151: goto st151;
		case 152: goto st152;
		case 153: goto st153;
		case 154: goto st154;
		case 155: goto st155;
		case 156: goto st156;
		case 157: goto st157;
		case 158: goto st158;
		case 159: goto st159;
		case 160: goto st160;
		case 161: goto st161;
		case 162: goto st162;
		case 163: goto st163;
		case 164: goto st164;
		case 165: goto st165;
		case 166: goto st166;
		case 167: goto st167;
		case 168: goto st168;
		case 169: goto st169;
		case 170: goto st170;
		case 171: goto st171;
		case 172: goto st172;
		case 173: goto st173;
		case 174: goto st174;
		case 175: goto st175;
		case 176: goto st176;
		case 177: goto st177;
		case 178: goto st178;
		case 179: goto st179;
		case 180: goto st180;
		case 181: goto st181;
		case 269: goto st269;
		case 182: goto st182;
		case 183: goto st183;
		case 184: goto st184;
		case 185: goto st185;
		case 186: goto st186;
		case 187: goto st187;
		case 188: goto st188;
		case 189: goto st189;
		case 190: goto st190;
		case 191: goto st191;
		case 192: goto st192;
		case 193: goto st193;
		case 194: goto st194;
		case 195: goto st195;
		case 196: goto st196;
		case 197: goto st197;
		case 198: goto st198;
		case 199: goto st199;
		case 200: goto st200;
		case 201: goto st201;
		case 202: goto st202;
		case 203: goto st203;
		case 204: goto st204;
		case 205: goto st205;
		case 206: goto st206;
		case 207: goto st207;
		case 208: goto st208;
		case 209: goto st209;
		case 210: goto st210;
		case 211: goto st211;
		case 212: goto st212;
		case 213: goto st213;
		case 214: goto st214;
		case 215: goto st215;
		case 216: goto st216;
		case 217: goto st217;
		case 218: goto st218;
		case 219: goto st219;
		case 220: goto st220;
		case 221: goto st221;
		case 222: goto st222;
		case 223: goto st223;
		case 224: goto st224;
		case 225: goto st225;
		case 226: goto st226;
		case 227: goto st227;
		case 228: goto st228;
		case 229: goto st229;
		case 230: goto st230;
		case 231: goto st231;
		case 232: goto st232;
		case 233: goto st233;
		case 234: goto st234;
		case 235: goto st235;
		case 236: goto st236;
		case 237: goto st237;
		case 238: goto st238;
		case 239: goto st239;
		case 240: goto st240;
		case 241: goto st241;
		case 242: goto st242;
		case 243: goto st243;
		case 244: goto st244;
		case 245: goto st245;
		case 246: goto st246;
		case 247: goto st247;
		case 248: goto st248;
		case 249: goto st249;
		case 250: goto st250;
		case 251: goto st251;
		case 252: goto st252;
		case 253: goto st253;
		case 254: goto st254;
		case 255: goto st255;
		case 256: goto st256;
		case 257: goto st257;
		case 258: goto st258;
		case 259: goto st259;
		case 260: goto st260;
		case 261: goto st261;
		case 262: goto st262;
		case 263: goto st263;
		case 264: goto st264;
		case 265: goto st265;
	default: break;
	}

	if ( ++p == pe )
		goto _test_eof;
_resume:
	switch ( cs )
	{
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	goto tr0;
tr0:
#line 404 "http_parser.rl"
	{
    p--;
    if (parser->type == HTTP_REQUEST) {
      {goto st268;}
    } else {
      {goto st269;}
    }
  }
	goto st266;
st266:
	if ( ++p == pe )
		goto _test_eof266;
case 266:
#line 492 "http_parser.c"
	goto st0;
st0:
cs = 0;
	goto _out;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	if ( (*p) == 48 )
		goto tr1;
	if ( (*p) < 65 ) {
		if ( 49 <= (*p) && (*p) <= 57 )
			goto tr3;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr3;
	} else
		goto tr3;
	goto st0;
tr1:
#line 253 "http_parser.rl"
	{
    parser->chunk_size *= 16;
    parser->chunk_size += unhex[(int)*p];
  }
	goto st3;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
#line 523 "http_parser.c"
	switch( (*p) ) {
		case 13: goto st4;
		case 48: goto tr1;
		case 59: goto st17;
	}
	if ( (*p) < 65 ) {
		if ( 49 <= (*p) && (*p) <= 57 )
			goto tr3;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr3;
	} else
		goto tr3;
	goto st0;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	if ( (*p) == 10 )
		goto st5;
	goto st0;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	switch( (*p) ) {
		case 13: goto st6;
		case 33: goto st7;
		case 124: goto st7;
		case 126: goto st7;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st7;
		} else if ( (*p) >= 35 )
			goto st7;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st7;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st7;
		} else
			goto st7;
	} else
		goto st7;
	goto st0;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	if ( (*p) == 10 )
		goto tr9;
	goto st0;
tr9:
	cs = 267;
#line 273 "http_parser.rl"
	{
    END_REQUEST;
    if (parser->type == HTTP_REQUEST) {
      cs = 268;
    } else {
      cs = 269;
    }
  }
	goto _again;
st267:
	if ( ++p == pe )
		goto _test_eof267;
case 267:
#line 596 "http_parser.c"
	goto st0;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	switch( (*p) ) {
		case 33: goto st7;
		case 58: goto st8;
		case 124: goto st7;
		case 126: goto st7;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st7;
		} else if ( (*p) >= 35 )
			goto st7;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st7;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st7;
		} else
			goto st7;
	} else
		goto st7;
	goto st0;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
	if ( (*p) == 13 )
		goto st4;
	goto st8;
tr3:
#line 253 "http_parser.rl"
	{
    parser->chunk_size *= 16;
    parser->chunk_size += unhex[(int)*p];
  }
	goto st9;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
#line 644 "http_parser.c"
	switch( (*p) ) {
		case 13: goto st10;
		case 59: goto st14;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr3;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr3;
	} else
		goto tr3;
	goto st0;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	if ( (*p) == 10 )
		goto st11;
	goto st0;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	goto tr14;
tr14:
#line 258 "http_parser.rl"
	{
    SKIP_BODY(MIN(parser->chunk_size, REMAINING));
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }

    p--; 
    if (parser->chunk_size > REMAINING) {
      {p++; cs = 12; goto _out;}
    } else {
      {goto st12;} 
    }
  }
	goto st12;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
#line 691 "http_parser.c"
	if ( (*p) == 13 )
		goto st13;
	goto st0;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	if ( (*p) == 10 )
		goto st2;
	goto st0;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	switch( (*p) ) {
		case 13: goto st10;
		case 32: goto st14;
		case 33: goto st15;
		case 59: goto st14;
		case 61: goto st16;
		case 124: goto st15;
		case 126: goto st15;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st15;
		} else if ( (*p) >= 35 )
			goto st15;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st15;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st15;
		} else
			goto st15;
	} else
		goto st15;
	goto st0;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	switch( (*p) ) {
		case 13: goto st10;
		case 33: goto st15;
		case 59: goto st14;
		case 61: goto st16;
		case 124: goto st15;
		case 126: goto st15;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st15;
		} else if ( (*p) >= 35 )
			goto st15;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st15;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st15;
		} else
			goto st15;
	} else
		goto st15;
	goto st0;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	switch( (*p) ) {
		case 13: goto st10;
		case 33: goto st16;
		case 59: goto st14;
		case 124: goto st16;
		case 126: goto st16;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st16;
		} else if ( (*p) >= 35 )
			goto st16;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st16;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st16;
		} else
			goto st16;
	} else
		goto st16;
	goto st0;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	switch( (*p) ) {
		case 13: goto st4;
		case 32: goto st17;
		case 33: goto st18;
		case 59: goto st17;
		case 61: goto st19;
		case 124: goto st18;
		case 126: goto st18;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st18;
		} else if ( (*p) >= 35 )
			goto st18;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st18;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st18;
		} else
			goto st18;
	} else
		goto st18;
	goto st0;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	switch( (*p) ) {
		case 13: goto st4;
		case 33: goto st18;
		case 59: goto st17;
		case 61: goto st19;
		case 124: goto st18;
		case 126: goto st18;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st18;
		} else if ( (*p) >= 35 )
			goto st18;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st18;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st18;
		} else
			goto st18;
	} else
		goto st18;
	goto st0;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	switch( (*p) ) {
		case 13: goto st4;
		case 33: goto st19;
		case 59: goto st17;
		case 124: goto st19;
		case 126: goto st19;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st19;
		} else if ( (*p) >= 35 )
			goto st19;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st19;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st19;
		} else
			goto st19;
	} else
		goto st19;
	goto st0;
tr45:
	cs = 268;
#line 203 "http_parser.rl"
	{
    if(parser->on_headers_complete) {
      callback_return_value = parser->on_headers_complete(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
#line 282 "http_parser.rl"
	{
    if (parser->transfer_encoding == HTTP_CHUNKED) {
      cs = 2;
    } else {
      /* this is pretty stupid. i'd prefer to combine this with skip_chunk_data */
      parser->chunk_size = parser->content_length;
      p += 1;  

      SKIP_BODY(MIN(REMAINING, parser->content_length));

      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }

      p--;
      if(parser->chunk_size > REMAINING) {
        {p++; goto _out;}
      }
    }
  }
	goto _again;
st268:
	if ( ++p == pe )
		goto _test_eof268;
case 268:
#line 921 "http_parser.c"
	switch( (*p) ) {
		case 67: goto tr310;
		case 68: goto tr311;
		case 71: goto tr312;
		case 72: goto tr313;
		case 76: goto tr314;
		case 77: goto tr315;
		case 79: goto tr316;
		case 80: goto tr317;
		case 84: goto tr318;
		case 85: goto tr319;
	}
	goto st0;
tr310:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st20;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
#line 951 "http_parser.c"
	if ( (*p) == 79 )
		goto st21;
	goto st0;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	if ( (*p) == 80 )
		goto st22;
	goto st0;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	if ( (*p) == 89 )
		goto st23;
	goto st0;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( (*p) == 32 )
		goto tr24;
	goto st0;
tr24:
#line 329 "http_parser.rl"
	{ parser->method = HTTP_COPY;      }
	goto st24;
tr154:
#line 330 "http_parser.rl"
	{ parser->method = HTTP_DELETE;    }
	goto st24;
tr157:
#line 331 "http_parser.rl"
	{ parser->method = HTTP_GET;       }
	goto st24;
tr161:
#line 332 "http_parser.rl"
	{ parser->method = HTTP_HEAD;      }
	goto st24;
tr165:
#line 333 "http_parser.rl"
	{ parser->method = HTTP_LOCK;      }
	goto st24;
tr171:
#line 334 "http_parser.rl"
	{ parser->method = HTTP_MKCOL;     }
	goto st24;
tr174:
#line 335 "http_parser.rl"
	{ parser->method = HTTP_MOVE;      }
	goto st24;
tr181:
#line 336 "http_parser.rl"
	{ parser->method = HTTP_OPTIONS;   }
	goto st24;
tr187:
#line 337 "http_parser.rl"
	{ parser->method = HTTP_POST;      }
	goto st24;
tr195:
#line 338 "http_parser.rl"
	{ parser->method = HTTP_PROPFIND;  }
	goto st24;
tr200:
#line 339 "http_parser.rl"
	{ parser->method = HTTP_PROPPATCH; }
	goto st24;
tr202:
#line 340 "http_parser.rl"
	{ parser->method = HTTP_PUT;       }
	goto st24;
tr207:
#line 341 "http_parser.rl"
	{ parser->method = HTTP_TRACE;     }
	goto st24;
tr213:
#line 342 "http_parser.rl"
	{ parser->method = HTTP_UNLOCK;    }
	goto st24;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
#line 1036 "http_parser.c"
	switch( (*p) ) {
		case 42: goto tr25;
		case 43: goto tr26;
		case 47: goto tr27;
		case 58: goto tr28;
	}
	if ( (*p) < 65 ) {
		if ( 45 <= (*p) && (*p) <= 57 )
			goto tr26;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr26;
	} else
		goto tr26;
	goto st0;
tr25:
#line 138 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
	goto st25;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
#line 1063 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr29;
		case 35: goto tr30;
	}
	goto st0;
tr29:
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st26;
tr124:
#line 123 "http_parser.rl"
	{
    parser->fragment_mark = p;
    parser->fragment_size = 0;
  }
#line 173 "http_parser.rl"
	{ 
    CALLBACK(fragment);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->fragment_mark = NULL;
    parser->fragment_size = 0;
  }
	goto st26;
tr127:
#line 173 "http_parser.rl"
	{ 
    CALLBACK(fragment);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->fragment_mark = NULL;
    parser->fragment_size = 0;
  }
	goto st26;
tr135:
#line 193 "http_parser.rl"
	{
    CALLBACK(path);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->path_mark = NULL;
    parser->path_size = 0;
  }
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st26;
tr141:
#line 128 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
#line 183 "http_parser.rl"
	{ 
    CALLBACK(query_string);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st26;
tr145:
#line 183 "http_parser.rl"
	{ 
    CALLBACK(query_string);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st26;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
#line 1185 "http_parser.c"
	if ( (*p) == 72 )
		goto st27;
	goto st0;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
	if ( (*p) == 84 )
		goto st28;
	goto st0;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
	if ( (*p) == 84 )
		goto st29;
	goto st0;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
	if ( (*p) == 80 )
		goto st30;
	goto st0;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
	if ( (*p) == 47 )
		goto st31;
	goto st0;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr36;
	goto st0;
tr36:
#line 243 "http_parser.rl"
	{
    parser->version_major *= 10;
    parser->version_major += *p - '0';
  }
	goto st32;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
#line 1235 "http_parser.c"
	if ( (*p) == 46 )
		goto st33;
	goto st0;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr38;
	goto st0;
tr38:
#line 248 "http_parser.rl"
	{
    parser->version_minor *= 10;
    parser->version_minor += *p - '0';
  }
	goto st34;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
#line 1257 "http_parser.c"
	if ( (*p) == 13 )
		goto st35;
	goto st0;
tr49:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st35;
tr52:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st35;
tr71:
#line 241 "http_parser.rl"
	{ parser->keep_alive = FALSE; }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st35;
tr81:
#line 240 "http_parser.rl"
	{ parser->keep_alive = TRUE; }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st35;
tr122:
#line 237 "http_parser.rl"
	{ parser->transfer_encoding = HTTP_IDENTITY; }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st35;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
#line 1336 "http_parser.c"
	if ( (*p) == 10 )
		goto st36;
	goto st0;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
	switch( (*p) ) {
		case 13: goto st37;
		case 33: goto tr42;
		case 67: goto tr43;
		case 84: goto tr44;
		case 99: goto tr43;
		case 116: goto tr44;
		case 124: goto tr42;
		case 126: goto tr42;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr42;
		} else if ( (*p) >= 35 )
			goto tr42;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr42;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr42;
		} else
			goto tr42;
	} else
		goto tr42;
	goto st0;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
	if ( (*p) == 10 )
		goto tr45;
	goto st0;
tr42:
#line 113 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st38;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
#line 1390 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
tr47:
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st39;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
#line 1431 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr49;
		case 32: goto st39;
	}
	goto tr48;
tr48:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st40;
st40:
	if ( ++p == pe )
		goto _test_eof40;
case 40:
#line 1448 "http_parser.c"
	if ( (*p) == 13 )
		goto tr52;
	goto st40;
tr43:
#line 113 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st41;
st41:
	if ( ++p == pe )
		goto _test_eof41;
case 41:
#line 1463 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 79: goto st42;
		case 111: goto st42;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st42:
	if ( ++p == pe )
		goto _test_eof42;
case 42:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st43;
		case 110: goto st43;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st43:
	if ( ++p == pe )
		goto _test_eof43;
case 43:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st44;
		case 84: goto st67;
		case 110: goto st44;
		case 116: goto st67;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st44:
	if ( ++p == pe )
		goto _test_eof44;
case 44:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 69: goto st45;
		case 101: goto st45;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st45:
	if ( ++p == pe )
		goto _test_eof45;
case 45:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 67: goto st46;
		case 99: goto st46;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st46:
	if ( ++p == pe )
		goto _test_eof46;
case 46:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 84: goto st47;
		case 116: goto st47;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st47:
	if ( ++p == pe )
		goto _test_eof47;
case 47:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 73: goto st48;
		case 105: goto st48;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st48:
	if ( ++p == pe )
		goto _test_eof48;
case 48:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 79: goto st49;
		case 111: goto st49;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st49:
	if ( ++p == pe )
		goto _test_eof49;
case 49:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st50;
		case 110: goto st50;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st50:
	if ( ++p == pe )
		goto _test_eof50;
case 50:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr63;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
tr63:
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st51;
st51:
	if ( ++p == pe )
		goto _test_eof51;
case 51:
#line 1776 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr49;
		case 32: goto st51;
		case 67: goto tr65;
		case 75: goto tr66;
		case 99: goto tr65;
		case 107: goto tr66;
	}
	goto tr48;
tr65:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st52;
st52:
	if ( ++p == pe )
		goto _test_eof52;
case 52:
#line 1797 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr52;
		case 76: goto st53;
		case 108: goto st53;
	}
	goto st40;
st53:
	if ( ++p == pe )
		goto _test_eof53;
case 53:
	switch( (*p) ) {
		case 13: goto tr52;
		case 79: goto st54;
		case 111: goto st54;
	}
	goto st40;
st54:
	if ( ++p == pe )
		goto _test_eof54;
case 54:
	switch( (*p) ) {
		case 13: goto tr52;
		case 83: goto st55;
		case 115: goto st55;
	}
	goto st40;
st55:
	if ( ++p == pe )
		goto _test_eof55;
case 55:
	switch( (*p) ) {
		case 13: goto tr52;
		case 69: goto st56;
		case 101: goto st56;
	}
	goto st40;
st56:
	if ( ++p == pe )
		goto _test_eof56;
case 56:
	if ( (*p) == 13 )
		goto tr71;
	goto st40;
tr66:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st57;
st57:
	if ( ++p == pe )
		goto _test_eof57;
case 57:
#line 1852 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr52;
		case 69: goto st58;
		case 101: goto st58;
	}
	goto st40;
st58:
	if ( ++p == pe )
		goto _test_eof58;
case 58:
	switch( (*p) ) {
		case 13: goto tr52;
		case 69: goto st59;
		case 101: goto st59;
	}
	goto st40;
st59:
	if ( ++p == pe )
		goto _test_eof59;
case 59:
	switch( (*p) ) {
		case 13: goto tr52;
		case 80: goto st60;
		case 112: goto st60;
	}
	goto st40;
st60:
	if ( ++p == pe )
		goto _test_eof60;
case 60:
	switch( (*p) ) {
		case 13: goto tr52;
		case 45: goto st61;
	}
	goto st40;
st61:
	if ( ++p == pe )
		goto _test_eof61;
case 61:
	switch( (*p) ) {
		case 13: goto tr52;
		case 65: goto st62;
		case 97: goto st62;
	}
	goto st40;
st62:
	if ( ++p == pe )
		goto _test_eof62;
case 62:
	switch( (*p) ) {
		case 13: goto tr52;
		case 76: goto st63;
		case 108: goto st63;
	}
	goto st40;
st63:
	if ( ++p == pe )
		goto _test_eof63;
case 63:
	switch( (*p) ) {
		case 13: goto tr52;
		case 73: goto st64;
		case 105: goto st64;
	}
	goto st40;
st64:
	if ( ++p == pe )
		goto _test_eof64;
case 64:
	switch( (*p) ) {
		case 13: goto tr52;
		case 86: goto st65;
		case 118: goto st65;
	}
	goto st40;
st65:
	if ( ++p == pe )
		goto _test_eof65;
case 65:
	switch( (*p) ) {
		case 13: goto tr52;
		case 69: goto st66;
		case 101: goto st66;
	}
	goto st40;
st66:
	if ( ++p == pe )
		goto _test_eof66;
case 66:
	if ( (*p) == 13 )
		goto tr81;
	goto st40;
st67:
	if ( ++p == pe )
		goto _test_eof67;
case 67:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 69: goto st68;
		case 101: goto st68;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st68:
	if ( ++p == pe )
		goto _test_eof68;
case 68:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st69;
		case 110: goto st69;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st69:
	if ( ++p == pe )
		goto _test_eof69;
case 69:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 84: goto st70;
		case 116: goto st70;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st70:
	if ( ++p == pe )
		goto _test_eof70;
case 70:
	switch( (*p) ) {
		case 33: goto st38;
		case 45: goto st71;
		case 46: goto st38;
		case 58: goto tr47;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else if ( (*p) >= 65 )
			goto st38;
	} else
		goto st38;
	goto st0;
st71:
	if ( ++p == pe )
		goto _test_eof71;
case 71:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 76: goto st72;
		case 108: goto st72;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st72:
	if ( ++p == pe )
		goto _test_eof72;
case 72:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 69: goto st73;
		case 101: goto st73;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st73:
	if ( ++p == pe )
		goto _test_eof73;
case 73:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st74;
		case 110: goto st74;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st74:
	if ( ++p == pe )
		goto _test_eof74;
case 74:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 71: goto st75;
		case 103: goto st75;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st75:
	if ( ++p == pe )
		goto _test_eof75;
case 75:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 84: goto st76;
		case 116: goto st76;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st76:
	if ( ++p == pe )
		goto _test_eof76;
case 76:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 72: goto st77;
		case 104: goto st77;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st77:
	if ( ++p == pe )
		goto _test_eof77;
case 77:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr92;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
tr92:
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st78;
st78:
	if ( ++p == pe )
		goto _test_eof78;
case 78:
#line 2286 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr49;
		case 32: goto st78;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr94;
	goto tr48;
tr94:
#line 223 "http_parser.rl"
	{
    if (parser->content_length > INT_MAX) {
      parser->error = TRUE;
      return 0;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st79;
tr95:
#line 223 "http_parser.rl"
	{
    if (parser->content_length > INT_MAX) {
      parser->error = TRUE;
      return 0;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
	goto st79;
st79:
	if ( ++p == pe )
		goto _test_eof79;
case 79:
#line 2325 "http_parser.c"
	if ( (*p) == 13 )
		goto tr52;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr95;
	goto st40;
tr44:
#line 113 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st80;
st80:
	if ( ++p == pe )
		goto _test_eof80;
case 80:
#line 2342 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 82: goto st81;
		case 114: goto st81;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st81:
	if ( ++p == pe )
		goto _test_eof81;
case 81:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 65: goto st82;
		case 97: goto st82;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 66 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st82:
	if ( ++p == pe )
		goto _test_eof82;
case 82:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st83;
		case 110: goto st83;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st83:
	if ( ++p == pe )
		goto _test_eof83;
case 83:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 83: goto st84;
		case 115: goto st84;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st84:
	if ( ++p == pe )
		goto _test_eof84;
case 84:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 70: goto st85;
		case 102: goto st85;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st85:
	if ( ++p == pe )
		goto _test_eof85;
case 85:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 69: goto st86;
		case 101: goto st86;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st86:
	if ( ++p == pe )
		goto _test_eof86;
case 86:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 82: goto st87;
		case 114: goto st87;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st87:
	if ( ++p == pe )
		goto _test_eof87;
case 87:
	switch( (*p) ) {
		case 33: goto st38;
		case 45: goto st88;
		case 46: goto st38;
		case 58: goto tr47;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else if ( (*p) >= 65 )
			goto st38;
	} else
		goto st38;
	goto st0;
st88:
	if ( ++p == pe )
		goto _test_eof88;
case 88:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 69: goto st89;
		case 101: goto st89;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st89:
	if ( ++p == pe )
		goto _test_eof89;
case 89:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st90;
		case 110: goto st90;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st90:
	if ( ++p == pe )
		goto _test_eof90;
case 90:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 67: goto st91;
		case 99: goto st91;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st91:
	if ( ++p == pe )
		goto _test_eof91;
case 91:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 79: goto st92;
		case 111: goto st92;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st92:
	if ( ++p == pe )
		goto _test_eof92;
case 92:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 68: goto st93;
		case 100: goto st93;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st93:
	if ( ++p == pe )
		goto _test_eof93;
case 93:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 73: goto st94;
		case 105: goto st94;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st94:
	if ( ++p == pe )
		goto _test_eof94;
case 94:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 78: goto st95;
		case 110: goto st95;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st95:
	if ( ++p == pe )
		goto _test_eof95;
case 95:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr47;
		case 71: goto st96;
		case 103: goto st96;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
st96:
	if ( ++p == pe )
		goto _test_eof96;
case 96:
	switch( (*p) ) {
		case 33: goto st38;
		case 58: goto tr112;
		case 124: goto st38;
		case 126: goto st38;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st38;
		} else if ( (*p) >= 35 )
			goto st38;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st38;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st38;
		} else
			goto st38;
	} else
		goto st38;
	goto st0;
tr112:
#line 238 "http_parser.rl"
	{ parser->transfer_encoding = HTTP_CHUNKED;  }
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st97;
st97:
	if ( ++p == pe )
		goto _test_eof97;
case 97:
#line 2862 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr49;
		case 32: goto st97;
		case 105: goto tr114;
	}
	goto tr48;
tr114:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st98;
st98:
	if ( ++p == pe )
		goto _test_eof98;
case 98:
#line 2880 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr52;
		case 100: goto st99;
	}
	goto st40;
st99:
	if ( ++p == pe )
		goto _test_eof99;
case 99:
	switch( (*p) ) {
		case 13: goto tr52;
		case 101: goto st100;
	}
	goto st40;
st100:
	if ( ++p == pe )
		goto _test_eof100;
case 100:
	switch( (*p) ) {
		case 13: goto tr52;
		case 110: goto st101;
	}
	goto st40;
st101:
	if ( ++p == pe )
		goto _test_eof101;
case 101:
	switch( (*p) ) {
		case 13: goto tr52;
		case 116: goto st102;
	}
	goto st40;
st102:
	if ( ++p == pe )
		goto _test_eof102;
case 102:
	switch( (*p) ) {
		case 13: goto tr52;
		case 105: goto st103;
	}
	goto st40;
st103:
	if ( ++p == pe )
		goto _test_eof103;
case 103:
	switch( (*p) ) {
		case 13: goto tr52;
		case 116: goto st104;
	}
	goto st40;
st104:
	if ( ++p == pe )
		goto _test_eof104;
case 104:
	switch( (*p) ) {
		case 13: goto tr52;
		case 121: goto st105;
	}
	goto st40;
st105:
	if ( ++p == pe )
		goto _test_eof105;
case 105:
	if ( (*p) == 13 )
		goto tr122;
	goto st40;
tr30:
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st106;
tr136:
#line 193 "http_parser.rl"
	{
    CALLBACK(path);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->path_mark = NULL;
    parser->path_size = 0;
  }
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st106;
tr142:
#line 128 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
#line 183 "http_parser.rl"
	{ 
    CALLBACK(query_string);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st106;
tr146:
#line 183 "http_parser.rl"
	{ 
    CALLBACK(query_string);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 163 "http_parser.rl"
	{ 
    CALLBACK(uri);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st106;
st106:
	if ( ++p == pe )
		goto _test_eof106;
case 106:
#line 3034 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr124;
		case 35: goto st0;
		case 37: goto tr125;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto tr123;
tr123:
#line 123 "http_parser.rl"
	{
    parser->fragment_mark = p;
    parser->fragment_size = 0;
  }
	goto st107;
st107:
	if ( ++p == pe )
		goto _test_eof107;
case 107:
#line 3057 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr127;
		case 35: goto st0;
		case 37: goto st108;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st107;
tr125:
#line 123 "http_parser.rl"
	{
    parser->fragment_mark = p;
    parser->fragment_size = 0;
  }
	goto st108;
st108:
	if ( ++p == pe )
		goto _test_eof108;
case 108:
#line 3080 "http_parser.c"
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st109;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st109;
	} else
		goto st109;
	goto st0;
st109:
	if ( ++p == pe )
		goto _test_eof109;
case 109:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st107;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st107;
	} else
		goto st107;
	goto st0;
tr26:
#line 138 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
	goto st110;
st110:
	if ( ++p == pe )
		goto _test_eof110;
case 110:
#line 3114 "http_parser.c"
	switch( (*p) ) {
		case 43: goto st110;
		case 58: goto st111;
	}
	if ( (*p) < 48 ) {
		if ( 45 <= (*p) && (*p) <= 46 )
			goto st110;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto st110;
		} else if ( (*p) >= 65 )
			goto st110;
	} else
		goto st110;
	goto st0;
tr28:
#line 138 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
	goto st111;
st111:
	if ( ++p == pe )
		goto _test_eof111;
case 111:
#line 3142 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr29;
		case 35: goto tr30;
		case 37: goto st112;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st111;
st112:
	if ( ++p == pe )
		goto _test_eof112;
case 112:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st113;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st113;
	} else
		goto st113;
	goto st0;
st113:
	if ( ++p == pe )
		goto _test_eof113;
case 113:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st111;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st111;
	} else
		goto st111;
	goto st0;
tr27:
#line 138 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
#line 133 "http_parser.rl"
	{
    parser->path_mark = p;
    parser->path_size = 0;
  }
	goto st114;
st114:
	if ( ++p == pe )
		goto _test_eof114;
case 114:
#line 3196 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr135;
		case 35: goto tr136;
		case 37: goto st115;
		case 60: goto st0;
		case 62: goto st0;
		case 63: goto tr138;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st114;
st115:
	if ( ++p == pe )
		goto _test_eof115;
case 115:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st116;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st116;
	} else
		goto st116;
	goto st0;
st116:
	if ( ++p == pe )
		goto _test_eof116;
case 116:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st114;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st114;
	} else
		goto st114;
	goto st0;
tr138:
#line 193 "http_parser.rl"
	{
    CALLBACK(path);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->path_mark = NULL;
    parser->path_size = 0;
  }
	goto st117;
st117:
	if ( ++p == pe )
		goto _test_eof117;
case 117:
#line 3251 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr141;
		case 35: goto tr142;
		case 37: goto tr143;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto tr140;
tr140:
#line 128 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
	goto st118;
st118:
	if ( ++p == pe )
		goto _test_eof118;
case 118:
#line 3274 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr145;
		case 35: goto tr146;
		case 37: goto st119;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st118;
tr143:
#line 128 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
	goto st119;
st119:
	if ( ++p == pe )
		goto _test_eof119;
case 119:
#line 3297 "http_parser.c"
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st120;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st120;
	} else
		goto st120;
	goto st0;
st120:
	if ( ++p == pe )
		goto _test_eof120;
case 120:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st118;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st118;
	} else
		goto st118;
	goto st0;
tr311:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st121;
st121:
	if ( ++p == pe )
		goto _test_eof121;
case 121:
#line 3336 "http_parser.c"
	if ( (*p) == 69 )
		goto st122;
	goto st0;
st122:
	if ( ++p == pe )
		goto _test_eof122;
case 122:
	if ( (*p) == 76 )
		goto st123;
	goto st0;
st123:
	if ( ++p == pe )
		goto _test_eof123;
case 123:
	if ( (*p) == 69 )
		goto st124;
	goto st0;
st124:
	if ( ++p == pe )
		goto _test_eof124;
case 124:
	if ( (*p) == 84 )
		goto st125;
	goto st0;
st125:
	if ( ++p == pe )
		goto _test_eof125;
case 125:
	if ( (*p) == 69 )
		goto st126;
	goto st0;
st126:
	if ( ++p == pe )
		goto _test_eof126;
case 126:
	if ( (*p) == 32 )
		goto tr154;
	goto st0;
tr312:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st127;
st127:
	if ( ++p == pe )
		goto _test_eof127;
case 127:
#line 3391 "http_parser.c"
	if ( (*p) == 69 )
		goto st128;
	goto st0;
st128:
	if ( ++p == pe )
		goto _test_eof128;
case 128:
	if ( (*p) == 84 )
		goto st129;
	goto st0;
st129:
	if ( ++p == pe )
		goto _test_eof129;
case 129:
	if ( (*p) == 32 )
		goto tr157;
	goto st0;
tr313:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st130;
st130:
	if ( ++p == pe )
		goto _test_eof130;
case 130:
#line 3425 "http_parser.c"
	if ( (*p) == 69 )
		goto st131;
	goto st0;
st131:
	if ( ++p == pe )
		goto _test_eof131;
case 131:
	if ( (*p) == 65 )
		goto st132;
	goto st0;
st132:
	if ( ++p == pe )
		goto _test_eof132;
case 132:
	if ( (*p) == 68 )
		goto st133;
	goto st0;
st133:
	if ( ++p == pe )
		goto _test_eof133;
case 133:
	if ( (*p) == 32 )
		goto tr161;
	goto st0;
tr314:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st134;
st134:
	if ( ++p == pe )
		goto _test_eof134;
case 134:
#line 3466 "http_parser.c"
	if ( (*p) == 79 )
		goto st135;
	goto st0;
st135:
	if ( ++p == pe )
		goto _test_eof135;
case 135:
	if ( (*p) == 67 )
		goto st136;
	goto st0;
st136:
	if ( ++p == pe )
		goto _test_eof136;
case 136:
	if ( (*p) == 75 )
		goto st137;
	goto st0;
st137:
	if ( ++p == pe )
		goto _test_eof137;
case 137:
	if ( (*p) == 32 )
		goto tr165;
	goto st0;
tr315:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st138;
st138:
	if ( ++p == pe )
		goto _test_eof138;
case 138:
#line 3507 "http_parser.c"
	switch( (*p) ) {
		case 75: goto st139;
		case 79: goto st143;
	}
	goto st0;
st139:
	if ( ++p == pe )
		goto _test_eof139;
case 139:
	if ( (*p) == 67 )
		goto st140;
	goto st0;
st140:
	if ( ++p == pe )
		goto _test_eof140;
case 140:
	if ( (*p) == 79 )
		goto st141;
	goto st0;
st141:
	if ( ++p == pe )
		goto _test_eof141;
case 141:
	if ( (*p) == 76 )
		goto st142;
	goto st0;
st142:
	if ( ++p == pe )
		goto _test_eof142;
case 142:
	if ( (*p) == 32 )
		goto tr171;
	goto st0;
st143:
	if ( ++p == pe )
		goto _test_eof143;
case 143:
	if ( (*p) == 86 )
		goto st144;
	goto st0;
st144:
	if ( ++p == pe )
		goto _test_eof144;
case 144:
	if ( (*p) == 69 )
		goto st145;
	goto st0;
st145:
	if ( ++p == pe )
		goto _test_eof145;
case 145:
	if ( (*p) == 32 )
		goto tr174;
	goto st0;
tr316:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st146;
st146:
	if ( ++p == pe )
		goto _test_eof146;
case 146:
#line 3578 "http_parser.c"
	if ( (*p) == 80 )
		goto st147;
	goto st0;
st147:
	if ( ++p == pe )
		goto _test_eof147;
case 147:
	if ( (*p) == 84 )
		goto st148;
	goto st0;
st148:
	if ( ++p == pe )
		goto _test_eof148;
case 148:
	if ( (*p) == 73 )
		goto st149;
	goto st0;
st149:
	if ( ++p == pe )
		goto _test_eof149;
case 149:
	if ( (*p) == 79 )
		goto st150;
	goto st0;
st150:
	if ( ++p == pe )
		goto _test_eof150;
case 150:
	if ( (*p) == 78 )
		goto st151;
	goto st0;
st151:
	if ( ++p == pe )
		goto _test_eof151;
case 151:
	if ( (*p) == 83 )
		goto st152;
	goto st0;
st152:
	if ( ++p == pe )
		goto _test_eof152;
case 152:
	if ( (*p) == 32 )
		goto tr181;
	goto st0;
tr317:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st153;
st153:
	if ( ++p == pe )
		goto _test_eof153;
case 153:
#line 3640 "http_parser.c"
	switch( (*p) ) {
		case 79: goto st154;
		case 82: goto st157;
		case 85: goto st169;
	}
	goto st0;
st154:
	if ( ++p == pe )
		goto _test_eof154;
case 154:
	if ( (*p) == 83 )
		goto st155;
	goto st0;
st155:
	if ( ++p == pe )
		goto _test_eof155;
case 155:
	if ( (*p) == 84 )
		goto st156;
	goto st0;
st156:
	if ( ++p == pe )
		goto _test_eof156;
case 156:
	if ( (*p) == 32 )
		goto tr187;
	goto st0;
st157:
	if ( ++p == pe )
		goto _test_eof157;
case 157:
	if ( (*p) == 79 )
		goto st158;
	goto st0;
st158:
	if ( ++p == pe )
		goto _test_eof158;
case 158:
	if ( (*p) == 80 )
		goto st159;
	goto st0;
st159:
	if ( ++p == pe )
		goto _test_eof159;
case 159:
	switch( (*p) ) {
		case 70: goto st160;
		case 80: goto st164;
	}
	goto st0;
st160:
	if ( ++p == pe )
		goto _test_eof160;
case 160:
	if ( (*p) == 73 )
		goto st161;
	goto st0;
st161:
	if ( ++p == pe )
		goto _test_eof161;
case 161:
	if ( (*p) == 78 )
		goto st162;
	goto st0;
st162:
	if ( ++p == pe )
		goto _test_eof162;
case 162:
	if ( (*p) == 68 )
		goto st163;
	goto st0;
st163:
	if ( ++p == pe )
		goto _test_eof163;
case 163:
	if ( (*p) == 32 )
		goto tr195;
	goto st0;
st164:
	if ( ++p == pe )
		goto _test_eof164;
case 164:
	if ( (*p) == 65 )
		goto st165;
	goto st0;
st165:
	if ( ++p == pe )
		goto _test_eof165;
case 165:
	if ( (*p) == 84 )
		goto st166;
	goto st0;
st166:
	if ( ++p == pe )
		goto _test_eof166;
case 166:
	if ( (*p) == 67 )
		goto st167;
	goto st0;
st167:
	if ( ++p == pe )
		goto _test_eof167;
case 167:
	if ( (*p) == 72 )
		goto st168;
	goto st0;
st168:
	if ( ++p == pe )
		goto _test_eof168;
case 168:
	if ( (*p) == 32 )
		goto tr200;
	goto st0;
st169:
	if ( ++p == pe )
		goto _test_eof169;
case 169:
	if ( (*p) == 84 )
		goto st170;
	goto st0;
st170:
	if ( ++p == pe )
		goto _test_eof170;
case 170:
	if ( (*p) == 32 )
		goto tr202;
	goto st0;
tr318:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st171;
st171:
	if ( ++p == pe )
		goto _test_eof171;
case 171:
#line 3784 "http_parser.c"
	if ( (*p) == 82 )
		goto st172;
	goto st0;
st172:
	if ( ++p == pe )
		goto _test_eof172;
case 172:
	if ( (*p) == 65 )
		goto st173;
	goto st0;
st173:
	if ( ++p == pe )
		goto _test_eof173;
case 173:
	if ( (*p) == 67 )
		goto st174;
	goto st0;
st174:
	if ( ++p == pe )
		goto _test_eof174;
case 174:
	if ( (*p) == 69 )
		goto st175;
	goto st0;
st175:
	if ( ++p == pe )
		goto _test_eof175;
case 175:
	if ( (*p) == 32 )
		goto tr207;
	goto st0;
tr319:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st176;
st176:
	if ( ++p == pe )
		goto _test_eof176;
case 176:
#line 3832 "http_parser.c"
	if ( (*p) == 78 )
		goto st177;
	goto st0;
st177:
	if ( ++p == pe )
		goto _test_eof177;
case 177:
	if ( (*p) == 76 )
		goto st178;
	goto st0;
st178:
	if ( ++p == pe )
		goto _test_eof178;
case 178:
	if ( (*p) == 79 )
		goto st179;
	goto st0;
st179:
	if ( ++p == pe )
		goto _test_eof179;
case 179:
	if ( (*p) == 67 )
		goto st180;
	goto st0;
st180:
	if ( ++p == pe )
		goto _test_eof180;
case 180:
	if ( (*p) == 75 )
		goto st181;
	goto st0;
st181:
	if ( ++p == pe )
		goto _test_eof181;
case 181:
	if ( (*p) == 32 )
		goto tr213;
	goto st0;
tr232:
	cs = 269;
#line 203 "http_parser.rl"
	{
    if(parser->on_headers_complete) {
      callback_return_value = parser->on_headers_complete(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
#line 282 "http_parser.rl"
	{
    if (parser->transfer_encoding == HTTP_CHUNKED) {
      cs = 2;
    } else {
      /* this is pretty stupid. i'd prefer to combine this with skip_chunk_data */
      parser->chunk_size = parser->content_length;
      p += 1;  

      SKIP_BODY(MIN(REMAINING, parser->content_length));

      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }

      p--;
      if(parser->chunk_size > REMAINING) {
        {p++; goto _out;}
      }
    }
  }
	goto _again;
st269:
	if ( ++p == pe )
		goto _test_eof269;
case 269:
#line 3910 "http_parser.c"
	if ( (*p) == 72 )
		goto tr320;
	goto st0;
tr320:
#line 213 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->error = TRUE;
        return 0;
      }
    }
  }
	goto st182;
st182:
	if ( ++p == pe )
		goto _test_eof182;
case 182:
#line 3930 "http_parser.c"
	if ( (*p) == 84 )
		goto st183;
	goto st0;
st183:
	if ( ++p == pe )
		goto _test_eof183;
case 183:
	if ( (*p) == 84 )
		goto st184;
	goto st0;
st184:
	if ( ++p == pe )
		goto _test_eof184;
case 184:
	if ( (*p) == 80 )
		goto st185;
	goto st0;
st185:
	if ( ++p == pe )
		goto _test_eof185;
case 185:
	if ( (*p) == 47 )
		goto st186;
	goto st0;
st186:
	if ( ++p == pe )
		goto _test_eof186;
case 186:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr218;
	goto st0;
tr218:
#line 243 "http_parser.rl"
	{
    parser->version_major *= 10;
    parser->version_major += *p - '0';
  }
	goto st187;
st187:
	if ( ++p == pe )
		goto _test_eof187;
case 187:
#line 3973 "http_parser.c"
	if ( (*p) == 46 )
		goto st188;
	goto st0;
st188:
	if ( ++p == pe )
		goto _test_eof188;
case 188:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr220;
	goto st0;
tr220:
#line 248 "http_parser.rl"
	{
    parser->version_minor *= 10;
    parser->version_minor += *p - '0';
  }
	goto st189;
st189:
	if ( ++p == pe )
		goto _test_eof189;
case 189:
#line 3995 "http_parser.c"
	if ( (*p) == 32 )
		goto st190;
	goto st0;
st190:
	if ( ++p == pe )
		goto _test_eof190;
case 190:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr222;
	goto st0;
tr222:
#line 232 "http_parser.rl"
	{
    parser->status_code *= 10;
    parser->status_code += *p - '0';
  }
	goto st191;
st191:
	if ( ++p == pe )
		goto _test_eof191;
case 191:
#line 4017 "http_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr223;
	goto st0;
tr223:
#line 232 "http_parser.rl"
	{
    parser->status_code *= 10;
    parser->status_code += *p - '0';
  }
	goto st192;
st192:
	if ( ++p == pe )
		goto _test_eof192;
case 192:
#line 4032 "http_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr224;
	goto st0;
tr224:
#line 232 "http_parser.rl"
	{
    parser->status_code *= 10;
    parser->status_code += *p - '0';
  }
	goto st193;
st193:
	if ( ++p == pe )
		goto _test_eof193;
case 193:
#line 4047 "http_parser.c"
	switch( (*p) ) {
		case 13: goto st194;
		case 32: goto st265;
	}
	goto st0;
tr236:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st194;
tr239:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st194;
tr258:
#line 241 "http_parser.rl"
	{ parser->keep_alive = FALSE; }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st194;
tr268:
#line 240 "http_parser.rl"
	{ parser->keep_alive = TRUE; }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st194;
tr309:
#line 237 "http_parser.rl"
	{ parser->transfer_encoding = HTTP_IDENTITY; }
#line 153 "http_parser.rl"
	{
    CALLBACK(header_value);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st194;
st194:
	if ( ++p == pe )
		goto _test_eof194;
case 194:
#line 4128 "http_parser.c"
	if ( (*p) == 10 )
		goto st195;
	goto st0;
st195:
	if ( ++p == pe )
		goto _test_eof195;
case 195:
	switch( (*p) ) {
		case 13: goto st196;
		case 33: goto tr229;
		case 67: goto tr230;
		case 84: goto tr231;
		case 99: goto tr230;
		case 116: goto tr231;
		case 124: goto tr229;
		case 126: goto tr229;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr229;
		} else if ( (*p) >= 35 )
			goto tr229;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr229;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr229;
		} else
			goto tr229;
	} else
		goto tr229;
	goto st0;
st196:
	if ( ++p == pe )
		goto _test_eof196;
case 196:
	if ( (*p) == 10 )
		goto tr232;
	goto st0;
tr229:
#line 113 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st197;
st197:
	if ( ++p == pe )
		goto _test_eof197;
case 197:
#line 4182 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
tr234:
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st198;
st198:
	if ( ++p == pe )
		goto _test_eof198;
case 198:
#line 4223 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr236;
		case 32: goto st198;
	}
	goto tr235;
tr235:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st199;
st199:
	if ( ++p == pe )
		goto _test_eof199;
case 199:
#line 4240 "http_parser.c"
	if ( (*p) == 13 )
		goto tr239;
	goto st199;
tr230:
#line 113 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st200;
st200:
	if ( ++p == pe )
		goto _test_eof200;
case 200:
#line 4255 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 79: goto st201;
		case 111: goto st201;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st201:
	if ( ++p == pe )
		goto _test_eof201;
case 201:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st202;
		case 110: goto st202;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st202:
	if ( ++p == pe )
		goto _test_eof202;
case 202:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st203;
		case 84: goto st226;
		case 110: goto st203;
		case 116: goto st226;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st203:
	if ( ++p == pe )
		goto _test_eof203;
case 203:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 69: goto st204;
		case 101: goto st204;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st204:
	if ( ++p == pe )
		goto _test_eof204;
case 204:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 67: goto st205;
		case 99: goto st205;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st205:
	if ( ++p == pe )
		goto _test_eof205;
case 205:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 84: goto st206;
		case 116: goto st206;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st206:
	if ( ++p == pe )
		goto _test_eof206;
case 206:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 73: goto st207;
		case 105: goto st207;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st207:
	if ( ++p == pe )
		goto _test_eof207;
case 207:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 79: goto st208;
		case 111: goto st208;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st208:
	if ( ++p == pe )
		goto _test_eof208;
case 208:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st209;
		case 110: goto st209;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st209:
	if ( ++p == pe )
		goto _test_eof209;
case 209:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr250;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
tr250:
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st210;
st210:
	if ( ++p == pe )
		goto _test_eof210;
case 210:
#line 4568 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr236;
		case 32: goto st210;
		case 67: goto tr252;
		case 75: goto tr253;
		case 99: goto tr252;
		case 107: goto tr253;
	}
	goto tr235;
tr252:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st211;
st211:
	if ( ++p == pe )
		goto _test_eof211;
case 211:
#line 4589 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr239;
		case 76: goto st212;
		case 108: goto st212;
	}
	goto st199;
st212:
	if ( ++p == pe )
		goto _test_eof212;
case 212:
	switch( (*p) ) {
		case 13: goto tr239;
		case 79: goto st213;
		case 111: goto st213;
	}
	goto st199;
st213:
	if ( ++p == pe )
		goto _test_eof213;
case 213:
	switch( (*p) ) {
		case 13: goto tr239;
		case 83: goto st214;
		case 115: goto st214;
	}
	goto st199;
st214:
	if ( ++p == pe )
		goto _test_eof214;
case 214:
	switch( (*p) ) {
		case 13: goto tr239;
		case 69: goto st215;
		case 101: goto st215;
	}
	goto st199;
st215:
	if ( ++p == pe )
		goto _test_eof215;
case 215:
	if ( (*p) == 13 )
		goto tr258;
	goto st199;
tr253:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st216;
st216:
	if ( ++p == pe )
		goto _test_eof216;
case 216:
#line 4644 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr239;
		case 69: goto st217;
		case 101: goto st217;
	}
	goto st199;
st217:
	if ( ++p == pe )
		goto _test_eof217;
case 217:
	switch( (*p) ) {
		case 13: goto tr239;
		case 69: goto st218;
		case 101: goto st218;
	}
	goto st199;
st218:
	if ( ++p == pe )
		goto _test_eof218;
case 218:
	switch( (*p) ) {
		case 13: goto tr239;
		case 80: goto st219;
		case 112: goto st219;
	}
	goto st199;
st219:
	if ( ++p == pe )
		goto _test_eof219;
case 219:
	switch( (*p) ) {
		case 13: goto tr239;
		case 45: goto st220;
	}
	goto st199;
st220:
	if ( ++p == pe )
		goto _test_eof220;
case 220:
	switch( (*p) ) {
		case 13: goto tr239;
		case 65: goto st221;
		case 97: goto st221;
	}
	goto st199;
st221:
	if ( ++p == pe )
		goto _test_eof221;
case 221:
	switch( (*p) ) {
		case 13: goto tr239;
		case 76: goto st222;
		case 108: goto st222;
	}
	goto st199;
st222:
	if ( ++p == pe )
		goto _test_eof222;
case 222:
	switch( (*p) ) {
		case 13: goto tr239;
		case 73: goto st223;
		case 105: goto st223;
	}
	goto st199;
st223:
	if ( ++p == pe )
		goto _test_eof223;
case 223:
	switch( (*p) ) {
		case 13: goto tr239;
		case 86: goto st224;
		case 118: goto st224;
	}
	goto st199;
st224:
	if ( ++p == pe )
		goto _test_eof224;
case 224:
	switch( (*p) ) {
		case 13: goto tr239;
		case 69: goto st225;
		case 101: goto st225;
	}
	goto st199;
st225:
	if ( ++p == pe )
		goto _test_eof225;
case 225:
	if ( (*p) == 13 )
		goto tr268;
	goto st199;
st226:
	if ( ++p == pe )
		goto _test_eof226;
case 226:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 69: goto st227;
		case 101: goto st227;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st227:
	if ( ++p == pe )
		goto _test_eof227;
case 227:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st228;
		case 110: goto st228;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st228:
	if ( ++p == pe )
		goto _test_eof228;
case 228:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 84: goto st229;
		case 116: goto st229;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st229:
	if ( ++p == pe )
		goto _test_eof229;
case 229:
	switch( (*p) ) {
		case 33: goto st197;
		case 45: goto st230;
		case 46: goto st197;
		case 58: goto tr234;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else if ( (*p) >= 65 )
			goto st197;
	} else
		goto st197;
	goto st0;
st230:
	if ( ++p == pe )
		goto _test_eof230;
case 230:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 76: goto st231;
		case 108: goto st231;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st231:
	if ( ++p == pe )
		goto _test_eof231;
case 231:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 69: goto st232;
		case 101: goto st232;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st232:
	if ( ++p == pe )
		goto _test_eof232;
case 232:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st233;
		case 110: goto st233;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st233:
	if ( ++p == pe )
		goto _test_eof233;
case 233:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 71: goto st234;
		case 103: goto st234;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st234:
	if ( ++p == pe )
		goto _test_eof234;
case 234:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 84: goto st235;
		case 116: goto st235;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st235:
	if ( ++p == pe )
		goto _test_eof235;
case 235:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 72: goto st236;
		case 104: goto st236;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st236:
	if ( ++p == pe )
		goto _test_eof236;
case 236:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr279;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
tr279:
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st237;
st237:
	if ( ++p == pe )
		goto _test_eof237;
case 237:
#line 5078 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr236;
		case 32: goto st237;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr281;
	goto tr235;
tr281:
#line 223 "http_parser.rl"
	{
    if (parser->content_length > INT_MAX) {
      parser->error = TRUE;
      return 0;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st238;
tr282:
#line 223 "http_parser.rl"
	{
    if (parser->content_length > INT_MAX) {
      parser->error = TRUE;
      return 0;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
	goto st238;
st238:
	if ( ++p == pe )
		goto _test_eof238;
case 238:
#line 5117 "http_parser.c"
	if ( (*p) == 13 )
		goto tr239;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr282;
	goto st199;
tr231:
#line 113 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st239;
st239:
	if ( ++p == pe )
		goto _test_eof239;
case 239:
#line 5134 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 82: goto st240;
		case 114: goto st240;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st240:
	if ( ++p == pe )
		goto _test_eof240;
case 240:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 65: goto st241;
		case 97: goto st241;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 66 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st241:
	if ( ++p == pe )
		goto _test_eof241;
case 241:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st242;
		case 110: goto st242;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st242:
	if ( ++p == pe )
		goto _test_eof242;
case 242:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 83: goto st243;
		case 115: goto st243;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st243:
	if ( ++p == pe )
		goto _test_eof243;
case 243:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 70: goto st244;
		case 102: goto st244;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st244:
	if ( ++p == pe )
		goto _test_eof244;
case 244:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 69: goto st245;
		case 101: goto st245;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st245:
	if ( ++p == pe )
		goto _test_eof245;
case 245:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 82: goto st246;
		case 114: goto st246;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st246:
	if ( ++p == pe )
		goto _test_eof246;
case 246:
	switch( (*p) ) {
		case 33: goto st197;
		case 45: goto st247;
		case 46: goto st197;
		case 58: goto tr234;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else if ( (*p) >= 65 )
			goto st197;
	} else
		goto st197;
	goto st0;
st247:
	if ( ++p == pe )
		goto _test_eof247;
case 247:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 69: goto st248;
		case 101: goto st248;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st248:
	if ( ++p == pe )
		goto _test_eof248;
case 248:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st249;
		case 110: goto st249;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st249:
	if ( ++p == pe )
		goto _test_eof249;
case 249:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 67: goto st250;
		case 99: goto st250;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st250:
	if ( ++p == pe )
		goto _test_eof250;
case 250:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 79: goto st251;
		case 111: goto st251;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st251:
	if ( ++p == pe )
		goto _test_eof251;
case 251:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 68: goto st252;
		case 100: goto st252;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st252:
	if ( ++p == pe )
		goto _test_eof252;
case 252:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 73: goto st253;
		case 105: goto st253;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st253:
	if ( ++p == pe )
		goto _test_eof253;
case 253:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 78: goto st254;
		case 110: goto st254;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st254:
	if ( ++p == pe )
		goto _test_eof254;
case 254:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr234;
		case 71: goto st255;
		case 103: goto st255;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
st255:
	if ( ++p == pe )
		goto _test_eof255;
case 255:
	switch( (*p) ) {
		case 33: goto st197;
		case 58: goto tr299;
		case 124: goto st197;
		case 126: goto st197;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st197;
		} else if ( (*p) >= 35 )
			goto st197;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st197;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st197;
		} else
			goto st197;
	} else
		goto st197;
	goto st0;
tr299:
#line 238 "http_parser.rl"
	{ parser->transfer_encoding = HTTP_CHUNKED;  }
#line 143 "http_parser.rl"
	{
    CALLBACK(header_field);
    if (callback_return_value != 0) {
      parser->error = TRUE;
      return 0;
    }
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st256;
st256:
	if ( ++p == pe )
		goto _test_eof256;
case 256:
#line 5654 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr236;
		case 32: goto st256;
		case 105: goto tr301;
	}
	goto tr235;
tr301:
#line 118 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st257;
st257:
	if ( ++p == pe )
		goto _test_eof257;
case 257:
#line 5672 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr239;
		case 100: goto st258;
	}
	goto st199;
st258:
	if ( ++p == pe )
		goto _test_eof258;
case 258:
	switch( (*p) ) {
		case 13: goto tr239;
		case 101: goto st259;
	}
	goto st199;
st259:
	if ( ++p == pe )
		goto _test_eof259;
case 259:
	switch( (*p) ) {
		case 13: goto tr239;
		case 110: goto st260;
	}
	goto st199;
st260:
	if ( ++p == pe )
		goto _test_eof260;
case 260:
	switch( (*p) ) {
		case 13: goto tr239;
		case 116: goto st261;
	}
	goto st199;
st261:
	if ( ++p == pe )
		goto _test_eof261;
case 261:
	switch( (*p) ) {
		case 13: goto tr239;
		case 105: goto st262;
	}
	goto st199;
st262:
	if ( ++p == pe )
		goto _test_eof262;
case 262:
	switch( (*p) ) {
		case 13: goto tr239;
		case 116: goto st263;
	}
	goto st199;
st263:
	if ( ++p == pe )
		goto _test_eof263;
case 263:
	switch( (*p) ) {
		case 13: goto tr239;
		case 121: goto st264;
	}
	goto st199;
st264:
	if ( ++p == pe )
		goto _test_eof264;
case 264:
	if ( (*p) == 13 )
		goto tr309;
	goto st199;
st265:
	if ( ++p == pe )
		goto _test_eof265;
case 265:
	if ( (*p) == 13 )
		goto st194;
	if ( (*p) > 9 ) {
		if ( 11 <= (*p) )
			goto st265;
	} else if ( (*p) >= 0 )
		goto st265;
	goto st0;
	}
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof266: cs = 266; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof267: cs = 267; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof268: cs = 268; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof33: cs = 33; goto _test_eof; 
	_test_eof34: cs = 34; goto _test_eof; 
	_test_eof35: cs = 35; goto _test_eof; 
	_test_eof36: cs = 36; goto _test_eof; 
	_test_eof37: cs = 37; goto _test_eof; 
	_test_eof38: cs = 38; goto _test_eof; 
	_test_eof39: cs = 39; goto _test_eof; 
	_test_eof40: cs = 40; goto _test_eof; 
	_test_eof41: cs = 41; goto _test_eof; 
	_test_eof42: cs = 42; goto _test_eof; 
	_test_eof43: cs = 43; goto _test_eof; 
	_test_eof44: cs = 44; goto _test_eof; 
	_test_eof45: cs = 45; goto _test_eof; 
	_test_eof46: cs = 46; goto _test_eof; 
	_test_eof47: cs = 47; goto _test_eof; 
	_test_eof48: cs = 48; goto _test_eof; 
	_test_eof49: cs = 49; goto _test_eof; 
	_test_eof50: cs = 50; goto _test_eof; 
	_test_eof51: cs = 51; goto _test_eof; 
	_test_eof52: cs = 52; goto _test_eof; 
	_test_eof53: cs = 53; goto _test_eof; 
	_test_eof54: cs = 54; goto _test_eof; 
	_test_eof55: cs = 55; goto _test_eof; 
	_test_eof56: cs = 56; goto _test_eof; 
	_test_eof57: cs = 57; goto _test_eof; 
	_test_eof58: cs = 58; goto _test_eof; 
	_test_eof59: cs = 59; goto _test_eof; 
	_test_eof60: cs = 60; goto _test_eof; 
	_test_eof61: cs = 61; goto _test_eof; 
	_test_eof62: cs = 62; goto _test_eof; 
	_test_eof63: cs = 63; goto _test_eof; 
	_test_eof64: cs = 64; goto _test_eof; 
	_test_eof65: cs = 65; goto _test_eof; 
	_test_eof66: cs = 66; goto _test_eof; 
	_test_eof67: cs = 67; goto _test_eof; 
	_test_eof68: cs = 68; goto _test_eof; 
	_test_eof69: cs = 69; goto _test_eof; 
	_test_eof70: cs = 70; goto _test_eof; 
	_test_eof71: cs = 71; goto _test_eof; 
	_test_eof72: cs = 72; goto _test_eof; 
	_test_eof73: cs = 73; goto _test_eof; 
	_test_eof74: cs = 74; goto _test_eof; 
	_test_eof75: cs = 75; goto _test_eof; 
	_test_eof76: cs = 76; goto _test_eof; 
	_test_eof77: cs = 77; goto _test_eof; 
	_test_eof78: cs = 78; goto _test_eof; 
	_test_eof79: cs = 79; goto _test_eof; 
	_test_eof80: cs = 80; goto _test_eof; 
	_test_eof81: cs = 81; goto _test_eof; 
	_test_eof82: cs = 82; goto _test_eof; 
	_test_eof83: cs = 83; goto _test_eof; 
	_test_eof84: cs = 84; goto _test_eof; 
	_test_eof85: cs = 85; goto _test_eof; 
	_test_eof86: cs = 86; goto _test_eof; 
	_test_eof87: cs = 87; goto _test_eof; 
	_test_eof88: cs = 88; goto _test_eof; 
	_test_eof89: cs = 89; goto _test_eof; 
	_test_eof90: cs = 90; goto _test_eof; 
	_test_eof91: cs = 91; goto _test_eof; 
	_test_eof92: cs = 92; goto _test_eof; 
	_test_eof93: cs = 93; goto _test_eof; 
	_test_eof94: cs = 94; goto _test_eof; 
	_test_eof95: cs = 95; goto _test_eof; 
	_test_eof96: cs = 96; goto _test_eof; 
	_test_eof97: cs = 97; goto _test_eof; 
	_test_eof98: cs = 98; goto _test_eof; 
	_test_eof99: cs = 99; goto _test_eof; 
	_test_eof100: cs = 100; goto _test_eof; 
	_test_eof101: cs = 101; goto _test_eof; 
	_test_eof102: cs = 102; goto _test_eof; 
	_test_eof103: cs = 103; goto _test_eof; 
	_test_eof104: cs = 104; goto _test_eof; 
	_test_eof105: cs = 105; goto _test_eof; 
	_test_eof106: cs = 106; goto _test_eof; 
	_test_eof107: cs = 107; goto _test_eof; 
	_test_eof108: cs = 108; goto _test_eof; 
	_test_eof109: cs = 109; goto _test_eof; 
	_test_eof110: cs = 110; goto _test_eof; 
	_test_eof111: cs = 111; goto _test_eof; 
	_test_eof112: cs = 112; goto _test_eof; 
	_test_eof113: cs = 113; goto _test_eof; 
	_test_eof114: cs = 114; goto _test_eof; 
	_test_eof115: cs = 115; goto _test_eof; 
	_test_eof116: cs = 116; goto _test_eof; 
	_test_eof117: cs = 117; goto _test_eof; 
	_test_eof118: cs = 118; goto _test_eof; 
	_test_eof119: cs = 119; goto _test_eof; 
	_test_eof120: cs = 120; goto _test_eof; 
	_test_eof121: cs = 121; goto _test_eof; 
	_test_eof122: cs = 122; goto _test_eof; 
	_test_eof123: cs = 123; goto _test_eof; 
	_test_eof124: cs = 124; goto _test_eof; 
	_test_eof125: cs = 125; goto _test_eof; 
	_test_eof126: cs = 126; goto _test_eof; 
	_test_eof127: cs = 127; goto _test_eof; 
	_test_eof128: cs = 128; goto _test_eof; 
	_test_eof129: cs = 129; goto _test_eof; 
	_test_eof130: cs = 130; goto _test_eof; 
	_test_eof131: cs = 131; goto _test_eof; 
	_test_eof132: cs = 132; goto _test_eof; 
	_test_eof133: cs = 133; goto _test_eof; 
	_test_eof134: cs = 134; goto _test_eof; 
	_test_eof135: cs = 135; goto _test_eof; 
	_test_eof136: cs = 136; goto _test_eof; 
	_test_eof137: cs = 137; goto _test_eof; 
	_test_eof138: cs = 138; goto _test_eof; 
	_test_eof139: cs = 139; goto _test_eof; 
	_test_eof140: cs = 140; goto _test_eof; 
	_test_eof141: cs = 141; goto _test_eof; 
	_test_eof142: cs = 142; goto _test_eof; 
	_test_eof143: cs = 143; goto _test_eof; 
	_test_eof144: cs = 144; goto _test_eof; 
	_test_eof145: cs = 145; goto _test_eof; 
	_test_eof146: cs = 146; goto _test_eof; 
	_test_eof147: cs = 147; goto _test_eof; 
	_test_eof148: cs = 148; goto _test_eof; 
	_test_eof149: cs = 149; goto _test_eof; 
	_test_eof150: cs = 150; goto _test_eof; 
	_test_eof151: cs = 151; goto _test_eof; 
	_test_eof152: cs = 152; goto _test_eof; 
	_test_eof153: cs = 153; goto _test_eof; 
	_test_eof154: cs = 154; goto _test_eof; 
	_test_eof155: cs = 155; goto _test_eof; 
	_test_eof156: cs = 156; goto _test_eof; 
	_test_eof157: cs = 157; goto _test_eof; 
	_test_eof158: cs = 158; goto _test_eof; 
	_test_eof159: cs = 159; goto _test_eof; 
	_test_eof160: cs = 160; goto _test_eof; 
	_test_eof161: cs = 161; goto _test_eof; 
	_test_eof162: cs = 162; goto _test_eof; 
	_test_eof163: cs = 163; goto _test_eof; 
	_test_eof164: cs = 164; goto _test_eof; 
	_test_eof165: cs = 165; goto _test_eof; 
	_test_eof166: cs = 166; goto _test_eof; 
	_test_eof167: cs = 167; goto _test_eof; 
	_test_eof168: cs = 168; goto _test_eof; 
	_test_eof169: cs = 169; goto _test_eof; 
	_test_eof170: cs = 170; goto _test_eof; 
	_test_eof171: cs = 171; goto _test_eof; 
	_test_eof172: cs = 172; goto _test_eof; 
	_test_eof173: cs = 173; goto _test_eof; 
	_test_eof174: cs = 174; goto _test_eof; 
	_test_eof175: cs = 175; goto _test_eof; 
	_test_eof176: cs = 176; goto _test_eof; 
	_test_eof177: cs = 177; goto _test_eof; 
	_test_eof178: cs = 178; goto _test_eof; 
	_test_eof179: cs = 179; goto _test_eof; 
	_test_eof180: cs = 180; goto _test_eof; 
	_test_eof181: cs = 181; goto _test_eof; 
	_test_eof269: cs = 269; goto _test_eof; 
	_test_eof182: cs = 182; goto _test_eof; 
	_test_eof183: cs = 183; goto _test_eof; 
	_test_eof184: cs = 184; goto _test_eof; 
	_test_eof185: cs = 185; goto _test_eof; 
	_test_eof186: cs = 186; goto _test_eof; 
	_test_eof187: cs = 187; goto _test_eof; 
	_test_eof188: cs = 188; goto _test_eof; 
	_test_eof189: cs = 189; goto _test_eof; 
	_test_eof190: cs = 190; goto _test_eof; 
	_test_eof191: cs = 191; goto _test_eof; 
	_test_eof192: cs = 192; goto _test_eof; 
	_test_eof193: cs = 193; goto _test_eof; 
	_test_eof194: cs = 194; goto _test_eof; 
	_test_eof195: cs = 195; goto _test_eof; 
	_test_eof196: cs = 196; goto _test_eof; 
	_test_eof197: cs = 197; goto _test_eof; 
	_test_eof198: cs = 198; goto _test_eof; 
	_test_eof199: cs = 199; goto _test_eof; 
	_test_eof200: cs = 200; goto _test_eof; 
	_test_eof201: cs = 201; goto _test_eof; 
	_test_eof202: cs = 202; goto _test_eof; 
	_test_eof203: cs = 203; goto _test_eof; 
	_test_eof204: cs = 204; goto _test_eof; 
	_test_eof205: cs = 205; goto _test_eof; 
	_test_eof206: cs = 206; goto _test_eof; 
	_test_eof207: cs = 207; goto _test_eof; 
	_test_eof208: cs = 208; goto _test_eof; 
	_test_eof209: cs = 209; goto _test_eof; 
	_test_eof210: cs = 210; goto _test_eof; 
	_test_eof211: cs = 211; goto _test_eof; 
	_test_eof212: cs = 212; goto _test_eof; 
	_test_eof213: cs = 213; goto _test_eof; 
	_test_eof214: cs = 214; goto _test_eof; 
	_test_eof215: cs = 215; goto _test_eof; 
	_test_eof216: cs = 216; goto _test_eof; 
	_test_eof217: cs = 217; goto _test_eof; 
	_test_eof218: cs = 218; goto _test_eof; 
	_test_eof219: cs = 219; goto _test_eof; 
	_test_eof220: cs = 220; goto _test_eof; 
	_test_eof221: cs = 221; goto _test_eof; 
	_test_eof222: cs = 222; goto _test_eof; 
	_test_eof223: cs = 223; goto _test_eof; 
	_test_eof224: cs = 224; goto _test_eof; 
	_test_eof225: cs = 225; goto _test_eof; 
	_test_eof226: cs = 226; goto _test_eof; 
	_test_eof227: cs = 227; goto _test_eof; 
	_test_eof228: cs = 228; goto _test_eof; 
	_test_eof229: cs = 229; goto _test_eof; 
	_test_eof230: cs = 230; goto _test_eof; 
	_test_eof231: cs = 231; goto _test_eof; 
	_test_eof232: cs = 232; goto _test_eof; 
	_test_eof233: cs = 233; goto _test_eof; 
	_test_eof234: cs = 234; goto _test_eof; 
	_test_eof235: cs = 235; goto _test_eof; 
	_test_eof236: cs = 236; goto _test_eof; 
	_test_eof237: cs = 237; goto _test_eof; 
	_test_eof238: cs = 238; goto _test_eof; 
	_test_eof239: cs = 239; goto _test_eof; 
	_test_eof240: cs = 240; goto _test_eof; 
	_test_eof241: cs = 241; goto _test_eof; 
	_test_eof242: cs = 242; goto _test_eof; 
	_test_eof243: cs = 243; goto _test_eof; 
	_test_eof244: cs = 244; goto _test_eof; 
	_test_eof245: cs = 245; goto _test_eof; 
	_test_eof246: cs = 246; goto _test_eof; 
	_test_eof247: cs = 247; goto _test_eof; 
	_test_eof248: cs = 248; goto _test_eof; 
	_test_eof249: cs = 249; goto _test_eof; 
	_test_eof250: cs = 250; goto _test_eof; 
	_test_eof251: cs = 251; goto _test_eof; 
	_test_eof252: cs = 252; goto _test_eof; 
	_test_eof253: cs = 253; goto _test_eof; 
	_test_eof254: cs = 254; goto _test_eof; 
	_test_eof255: cs = 255; goto _test_eof; 
	_test_eof256: cs = 256; goto _test_eof; 
	_test_eof257: cs = 257; goto _test_eof; 
	_test_eof258: cs = 258; goto _test_eof; 
	_test_eof259: cs = 259; goto _test_eof; 
	_test_eof260: cs = 260; goto _test_eof; 
	_test_eof261: cs = 261; goto _test_eof; 
	_test_eof262: cs = 262; goto _test_eof; 
	_test_eof263: cs = 263; goto _test_eof; 
	_test_eof264: cs = 264; goto _test_eof; 
	_test_eof265: cs = 265; goto _test_eof; 

	_test_eof: {}
	_out: {}
	}
#line 469 "http_parser.rl"

  parser->cs = cs;

  CALLBACK(header_field);
  CALLBACK(header_value);
  CALLBACK(fragment);
  CALLBACK(query_string);
  CALLBACK(path);
  CALLBACK(uri);

  assert(p <= pe && "buffer overflow after parsing execute");
  return(p - buffer);
}

int
http_parser_has_error (http_parser *parser) 
{
  if (parser->error) return TRUE;
  return parser->cs == http_parser_error;
}

int
http_parser_should_keep_alive (http_parser *parser)
{
  if (parser->keep_alive == -1)
    if (parser->version_major == 1)
      return (parser->version_minor != 0);
    else if (parser->version_major == 0)
      return FALSE;
    else
      return TRUE;
  else
    return parser->keep_alive;
}
