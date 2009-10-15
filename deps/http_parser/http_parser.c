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

/* parser->flags */
#define EATING      0x01
#define ERROR       0x02
#define CHUNKED     0x04
#define EAT_FOREVER 0x10

static int unhex[] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     };

#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#undef NULL
#define NULL ((void*)(0))

#define MAX_FIELD_SIZE (80*1024)

#define REMAINING (unsigned long)(pe - p)
#define CALLBACK(FOR)                                                \
do {                                                                 \
  if (parser->FOR##_mark) {                                          \
    parser->FOR##_size += p - parser->FOR##_mark;                    \
    if (parser->FOR##_size > MAX_FIELD_SIZE) {                       \
      parser->flags |= ERROR;                                        \
      return;                                                        \
    }                                                                \
    if (parser->on_##FOR) {                                          \
      callback_return_value = parser->on_##FOR(parser,               \
        parser->FOR##_mark,                                          \
        p - parser->FOR##_mark);                                     \
    }                                                                \
    if (callback_return_value != 0) {                                \
      parser->flags |= ERROR;                                        \
      return;                                                        \
    }                                                                \
  }                                                                  \
} while(0)

#define RESET_PARSER(parser)                                         \
  parser->chunk_size = 0;                                            \
  parser->flags = 0;                                                 \
  parser->header_field_mark = NULL;                                  \
  parser->header_value_mark = NULL;                                  \
  parser->query_string_mark = NULL;                                  \
  parser->path_mark = NULL;                                          \
  parser->uri_mark = NULL;                                           \
  parser->fragment_mark = NULL;                                      \
  parser->status_code = 0;                                           \
  parser->method = 0;                                                \
  parser->version = HTTP_VERSION_OTHER;                              \
  parser->keep_alive = -1;                                           \
  parser->content_length = -1;                                       \
  parser->body_read = 0

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
      parser->flags &= ~EATING;                                      \
      if (!(parser->flags & CHUNKED)) {                              \
        END_REQUEST;                                                 \
      }                                                              \
    } else {                                                         \
      parser->flags |= EATING;                                       \
    }                                                                \
  }                                                                  \
} while (0)

#line 411 "http_parser.rl"



#line 126 "http_parser.c"
static const int http_parser_start = 1;
static const int http_parser_first_final = 280;
static const int http_parser_error = 0;

static const int http_parser_en_ChunkedBody = 2;
static const int http_parser_en_ChunkedBody_chunk_chunk_end = 13;
static const int http_parser_en_Requests = 282;
static const int http_parser_en_Responses = 283;
static const int http_parser_en_main = 1;

#line 414 "http_parser.rl"

void
http_parser_init (http_parser *parser, enum http_parser_type type)
{
  int cs = 0;
  
#line 144 "http_parser.c"
	{
	cs = http_parser_start;
	}
#line 420 "http_parser.rl"
  parser->cs = cs;
  parser->type = type;

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
void
http_parser_execute (http_parser *parser, const char *buffer, size_t len)
{
  size_t tmp; // REMOVE ME this is extremely hacky
  int callback_return_value = 0;
  const char *p, *pe, *eof;
  int cs = parser->cs;

  p = buffer;
  pe = buffer+len;
  eof = len ? NULL : pe;

  if (parser->flags & EAT_FOREVER) {
    if (len == 0) {
      if (parser->on_message_complete) {
        callback_return_value = parser->on_message_complete(parser);
        if (callback_return_value != 0) parser->flags |= ERROR;
      }
    } else {
      if (parser->on_body) {
        callback_return_value = parser->on_body(parser, p, len);
        if (callback_return_value != 0) parser->flags |= ERROR;
      }
    }
    return;
  }

  if (0 < parser->chunk_size && (parser->flags & EATING)) {
    /* eat body */
    SKIP_BODY(MIN(len, parser->chunk_size));
    if (callback_return_value != 0) {
      parser->flags |= ERROR;
      return;
    }
  }

  if (parser->header_field_mark)   parser->header_field_mark   = buffer;
  if (parser->header_value_mark)   parser->header_value_mark   = buffer;
  if (parser->fragment_mark)       parser->fragment_mark       = buffer;
  if (parser->query_string_mark)   parser->query_string_mark   = buffer;
  if (parser->path_mark)           parser->path_mark           = buffer;
  if (parser->uri_mark)            parser->uri_mark            = buffer;

  
#line 211 "http_parser.c"
	{
	if ( p == pe )
		goto _test_eof;
	goto _resume;

_again:
	switch ( cs ) {
		case 1: goto st1;
		case 280: goto st280;
		case 0: goto st0;
		case 2: goto st2;
		case 3: goto st3;
		case 4: goto st4;
		case 5: goto st5;
		case 6: goto st6;
		case 281: goto st281;
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
		case 20: goto st20;
		case 21: goto st21;
		case 282: goto st282;
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
		case 182: goto st182;
		case 183: goto st183;
		case 184: goto st184;
		case 185: goto st185;
		case 186: goto st186;
		case 187: goto st187;
		case 188: goto st188;
		case 189: goto st189;
		case 283: goto st283;
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
		case 266: goto st266;
		case 267: goto st267;
		case 268: goto st268;
		case 269: goto st269;
		case 270: goto st270;
		case 271: goto st271;
		case 272: goto st272;
		case 273: goto st273;
		case 274: goto st274;
		case 275: goto st275;
		case 276: goto st276;
		case 277: goto st277;
		case 278: goto st278;
		case 279: goto st279;
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
#line 402 "http_parser.rl"
	{
    p--;
    if (parser->type == HTTP_REQUEST) {
      {goto st282;}
    } else {
      {goto st283;}
    }
  }
	goto st280;
st280:
	if ( ++p == pe )
		goto _test_eof280;
case 280:
#line 531 "http_parser.c"
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
#line 233 "http_parser.rl"
	{
    parser->chunk_size *= 16;
    parser->chunk_size += unhex[(int)*p];
  }
	goto st3;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
#line 562 "http_parser.c"
	switch( (*p) ) {
		case 13: goto st4;
		case 32: goto st9;
		case 48: goto tr1;
		case 59: goto st19;
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
		goto tr10;
	goto st0;
tr10:
	cs = 281;
#line 253 "http_parser.rl"
	{
    END_REQUEST;
    if (parser->type == HTTP_REQUEST) {
      cs = 282;
    } else {
      cs = 283;
    }
  }
	goto _again;
st281:
	if ( ++p == pe )
		goto _test_eof281;
case 281:
#line 636 "http_parser.c"
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
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
	switch( (*p) ) {
		case 13: goto st4;
		case 32: goto st9;
	}
	goto st0;
tr3:
#line 233 "http_parser.rl"
	{
    parser->chunk_size *= 16;
    parser->chunk_size += unhex[(int)*p];
  }
	goto st10;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
#line 693 "http_parser.c"
	switch( (*p) ) {
		case 13: goto st11;
		case 32: goto st15;
		case 59: goto st16;
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
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	if ( (*p) == 10 )
		goto st12;
	goto st0;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	goto tr16;
tr16:
#line 238 "http_parser.rl"
	{
    SKIP_BODY(MIN(parser->chunk_size, REMAINING));
    if (callback_return_value != 0) {
      parser->flags |= ERROR;
      return;
    }

    p--;
    if (parser->chunk_size > REMAINING) {
      {p++; cs = 13; goto _out;}
    } else {
      {goto st13;}
    }
  }
	goto st13;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
#line 741 "http_parser.c"
	if ( (*p) == 13 )
		goto st14;
	goto st0;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	if ( (*p) == 10 )
		goto st2;
	goto st0;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
	switch( (*p) ) {
		case 13: goto st11;
		case 32: goto st15;
	}
	goto st0;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	switch( (*p) ) {
		case 13: goto st11;
		case 32: goto st16;
		case 33: goto st17;
		case 59: goto st16;
		case 61: goto st18;
		case 124: goto st17;
		case 126: goto st17;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st17;
		} else if ( (*p) >= 35 )
			goto st17;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st17;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st17;
		} else
			goto st17;
	} else
		goto st17;
	goto st0;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	switch( (*p) ) {
		case 13: goto st11;
		case 33: goto st17;
		case 59: goto st16;
		case 61: goto st18;
		case 124: goto st17;
		case 126: goto st17;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st17;
		} else if ( (*p) >= 35 )
			goto st17;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st17;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st17;
		} else
			goto st17;
	} else
		goto st17;
	goto st0;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	switch( (*p) ) {
		case 13: goto st11;
		case 33: goto st18;
		case 59: goto st16;
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
		case 32: goto st19;
		case 33: goto st20;
		case 59: goto st19;
		case 61: goto st21;
		case 124: goto st20;
		case 126: goto st20;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st20;
		} else if ( (*p) >= 35 )
			goto st20;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st20;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st20;
		} else
			goto st20;
	} else
		goto st20;
	goto st0;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
	switch( (*p) ) {
		case 13: goto st4;
		case 33: goto st20;
		case 59: goto st19;
		case 61: goto st21;
		case 124: goto st20;
		case 126: goto st20;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st20;
		} else if ( (*p) >= 35 )
			goto st20;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st20;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st20;
		} else
			goto st20;
	} else
		goto st20;
	goto st0;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	switch( (*p) ) {
		case 13: goto st4;
		case 33: goto st21;
		case 59: goto st19;
		case 124: goto st21;
		case 126: goto st21;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st21;
		} else if ( (*p) >= 35 )
			goto st21;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st21;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st21;
		} else
			goto st21;
	} else
		goto st21;
	goto st0;
tr50:
	cs = 282;
#line 189 "http_parser.rl"
	{
    if(parser->on_headers_complete) {
      callback_return_value = parser->on_headers_complete(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
#line 262 "http_parser.rl"
	{
    if (parser->flags & CHUNKED) {
      cs = 2;
    } else {
      /* this is pretty stupid. i'd prefer to combine this with
       * skip_chunk_data */
      if (parser->content_length < 0) {
        /* If we didn't get a content length; if not keep-alive
         * just read body until EOF */
        if (!http_parser_should_keep_alive(parser)) {
          parser->flags |= EAT_FOREVER;
          parser->chunk_size = REMAINING;
        } else {
          /* Otherwise, if keep-alive, then assume the message
           * has no body. */
          parser->chunk_size = parser->content_length = 0;
        }
      } else {
        parser->chunk_size = parser->content_length;
      }
      p += 1;

      SKIP_BODY(MIN(REMAINING, parser->chunk_size));

      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }

      p--;
      if(parser->chunk_size > REMAINING) {
        {p++; goto _out;}
      }
    }
  }
	goto _again;
st282:
	if ( ++p == pe )
		goto _test_eof282;
case 282:
#line 994 "http_parser.c"
	switch( (*p) ) {
		case 67: goto tr330;
		case 68: goto tr331;
		case 71: goto tr332;
		case 72: goto tr333;
		case 76: goto tr334;
		case 77: goto tr335;
		case 79: goto tr336;
		case 80: goto tr337;
		case 84: goto tr338;
		case 85: goto tr339;
	}
	goto st0;
tr330:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st22;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
#line 1024 "http_parser.c"
	if ( (*p) == 79 )
		goto st23;
	goto st0;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( (*p) == 80 )
		goto st24;
	goto st0;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	if ( (*p) == 89 )
		goto st25;
	goto st0;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	if ( (*p) == 32 )
		goto tr26;
	goto st0;
tr26:
#line 323 "http_parser.rl"
	{ parser->method = HTTP_COPY;      }
	goto st26;
tr165:
#line 324 "http_parser.rl"
	{ parser->method = HTTP_DELETE;    }
	goto st26;
tr168:
#line 325 "http_parser.rl"
	{ parser->method = HTTP_GET;       }
	goto st26;
tr172:
#line 326 "http_parser.rl"
	{ parser->method = HTTP_HEAD;      }
	goto st26;
tr176:
#line 327 "http_parser.rl"
	{ parser->method = HTTP_LOCK;      }
	goto st26;
tr182:
#line 328 "http_parser.rl"
	{ parser->method = HTTP_MKCOL;     }
	goto st26;
tr185:
#line 329 "http_parser.rl"
	{ parser->method = HTTP_MOVE;      }
	goto st26;
tr192:
#line 330 "http_parser.rl"
	{ parser->method = HTTP_OPTIONS;   }
	goto st26;
tr198:
#line 331 "http_parser.rl"
	{ parser->method = HTTP_POST;      }
	goto st26;
tr206:
#line 332 "http_parser.rl"
	{ parser->method = HTTP_PROPFIND;  }
	goto st26;
tr211:
#line 333 "http_parser.rl"
	{ parser->method = HTTP_PROPPATCH; }
	goto st26;
tr213:
#line 334 "http_parser.rl"
	{ parser->method = HTTP_PUT;       }
	goto st26;
tr218:
#line 335 "http_parser.rl"
	{ parser->method = HTTP_TRACE;     }
	goto st26;
tr224:
#line 336 "http_parser.rl"
	{ parser->method = HTTP_UNLOCK;    }
	goto st26;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
#line 1109 "http_parser.c"
	switch( (*p) ) {
		case 42: goto tr27;
		case 43: goto tr28;
		case 47: goto tr29;
		case 58: goto tr30;
	}
	if ( (*p) < 65 ) {
		if ( 45 <= (*p) && (*p) <= 57 )
			goto tr28;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr28;
	} else
		goto tr28;
	goto st0;
tr27:
#line 148 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
	goto st27;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
#line 1136 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr31;
		case 35: goto tr32;
	}
	goto st0;
tr31:
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st28;
tr135:
#line 133 "http_parser.rl"
	{
    parser->fragment_mark = p;
    parser->fragment_size = 0;
  }
#line 171 "http_parser.rl"
	{
    CALLBACK(fragment);
    parser->fragment_mark = NULL;
    parser->fragment_size = 0;
  }
	goto st28;
tr138:
#line 171 "http_parser.rl"
	{
    CALLBACK(fragment);
    parser->fragment_mark = NULL;
    parser->fragment_size = 0;
  }
	goto st28;
tr146:
#line 183 "http_parser.rl"
	{
    CALLBACK(path);
    parser->path_mark = NULL;
    parser->path_size = 0;
  }
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st28;
tr152:
#line 138 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
#line 177 "http_parser.rl"
	{
    CALLBACK(query_string);
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st28;
tr156:
#line 177 "http_parser.rl"
	{
    CALLBACK(query_string);
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st28;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
#line 1222 "http_parser.c"
	if ( (*p) == 72 )
		goto st29;
	goto st0;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
	if ( (*p) == 84 )
		goto st30;
	goto st0;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
	if ( (*p) == 84 )
		goto st31;
	goto st0;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
	if ( (*p) == 80 )
		goto st32;
	goto st0;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
	if ( (*p) == 47 )
		goto st33;
	goto st0;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
	switch( (*p) ) {
		case 48: goto st34;
		case 49: goto st108;
	}
	if ( 50 <= (*p) && (*p) <= 57 )
		goto st112;
	goto st0;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
	if ( (*p) == 46 )
		goto st35;
	goto st0;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
	if ( (*p) == 57 )
		goto st107;
	if ( 48 <= (*p) && (*p) <= 56 )
		goto st36;
	goto st0;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
	if ( (*p) == 13 )
		goto st37;
	goto st0;
tr54:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st37;
tr57:
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st37;
tr76:
#line 227 "http_parser.rl"
	{ parser->keep_alive = 0; }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st37;
tr86:
#line 226 "http_parser.rl"
	{ parser->keep_alive = 1; }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st37;
tr126:
#line 224 "http_parser.rl"
	{ parser->flags |= CHUNKED;  }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st37;
tr127:
#line 231 "http_parser.rl"
	{ parser->version = HTTP_VERSION_09; }
	goto st37;
tr131:
#line 230 "http_parser.rl"
	{ parser->version = HTTP_VERSION_10; }
	goto st37;
tr132:
#line 229 "http_parser.rl"
	{ parser->version = HTTP_VERSION_11; }
	goto st37;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
#line 1355 "http_parser.c"
	if ( (*p) == 10 )
		goto st38;
	goto st0;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
	switch( (*p) ) {
		case 13: goto st39;
		case 33: goto tr47;
		case 67: goto tr48;
		case 84: goto tr49;
		case 99: goto tr48;
		case 116: goto tr49;
		case 124: goto tr47;
		case 126: goto tr47;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr47;
		} else if ( (*p) >= 35 )
			goto tr47;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr47;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr47;
		} else
			goto tr47;
	} else
		goto tr47;
	goto st0;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
	if ( (*p) == 10 )
		goto tr50;
	goto st0;
tr47:
#line 123 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st40;
st40:
	if ( ++p == pe )
		goto _test_eof40;
case 40:
#line 1409 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
tr52:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st41;
st41:
	if ( ++p == pe )
		goto _test_eof41;
case 41:
#line 1446 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr54;
		case 32: goto st41;
	}
	goto tr53;
tr53:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st42;
st42:
	if ( ++p == pe )
		goto _test_eof42;
case 42:
#line 1463 "http_parser.c"
	if ( (*p) == 13 )
		goto tr57;
	goto st42;
tr48:
#line 123 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st43;
st43:
	if ( ++p == pe )
		goto _test_eof43;
case 43:
#line 1478 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 79: goto st44;
		case 111: goto st44;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st44:
	if ( ++p == pe )
		goto _test_eof44;
case 44:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st45;
		case 110: goto st45;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st45:
	if ( ++p == pe )
		goto _test_eof45;
case 45:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st46;
		case 84: goto st69;
		case 110: goto st46;
		case 116: goto st69;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st46:
	if ( ++p == pe )
		goto _test_eof46;
case 46:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 69: goto st47;
		case 101: goto st47;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st47:
	if ( ++p == pe )
		goto _test_eof47;
case 47:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 67: goto st48;
		case 99: goto st48;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st48:
	if ( ++p == pe )
		goto _test_eof48;
case 48:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 84: goto st49;
		case 116: goto st49;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st49:
	if ( ++p == pe )
		goto _test_eof49;
case 49:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 73: goto st50;
		case 105: goto st50;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st50:
	if ( ++p == pe )
		goto _test_eof50;
case 50:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 79: goto st51;
		case 111: goto st51;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st51:
	if ( ++p == pe )
		goto _test_eof51;
case 51:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st52;
		case 110: goto st52;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st52:
	if ( ++p == pe )
		goto _test_eof52;
case 52:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr68;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
tr68:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st53;
st53:
	if ( ++p == pe )
		goto _test_eof53;
case 53:
#line 1787 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr54;
		case 32: goto st53;
		case 67: goto tr70;
		case 75: goto tr71;
		case 99: goto tr70;
		case 107: goto tr71;
	}
	goto tr53;
tr70:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st54;
st54:
	if ( ++p == pe )
		goto _test_eof54;
case 54:
#line 1808 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr57;
		case 76: goto st55;
		case 108: goto st55;
	}
	goto st42;
st55:
	if ( ++p == pe )
		goto _test_eof55;
case 55:
	switch( (*p) ) {
		case 13: goto tr57;
		case 79: goto st56;
		case 111: goto st56;
	}
	goto st42;
st56:
	if ( ++p == pe )
		goto _test_eof56;
case 56:
	switch( (*p) ) {
		case 13: goto tr57;
		case 83: goto st57;
		case 115: goto st57;
	}
	goto st42;
st57:
	if ( ++p == pe )
		goto _test_eof57;
case 57:
	switch( (*p) ) {
		case 13: goto tr57;
		case 69: goto st58;
		case 101: goto st58;
	}
	goto st42;
st58:
	if ( ++p == pe )
		goto _test_eof58;
case 58:
	if ( (*p) == 13 )
		goto tr76;
	goto st42;
tr71:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st59;
st59:
	if ( ++p == pe )
		goto _test_eof59;
case 59:
#line 1863 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr57;
		case 69: goto st60;
		case 101: goto st60;
	}
	goto st42;
st60:
	if ( ++p == pe )
		goto _test_eof60;
case 60:
	switch( (*p) ) {
		case 13: goto tr57;
		case 69: goto st61;
		case 101: goto st61;
	}
	goto st42;
st61:
	if ( ++p == pe )
		goto _test_eof61;
case 61:
	switch( (*p) ) {
		case 13: goto tr57;
		case 80: goto st62;
		case 112: goto st62;
	}
	goto st42;
st62:
	if ( ++p == pe )
		goto _test_eof62;
case 62:
	switch( (*p) ) {
		case 13: goto tr57;
		case 45: goto st63;
	}
	goto st42;
st63:
	if ( ++p == pe )
		goto _test_eof63;
case 63:
	switch( (*p) ) {
		case 13: goto tr57;
		case 65: goto st64;
		case 97: goto st64;
	}
	goto st42;
st64:
	if ( ++p == pe )
		goto _test_eof64;
case 64:
	switch( (*p) ) {
		case 13: goto tr57;
		case 76: goto st65;
		case 108: goto st65;
	}
	goto st42;
st65:
	if ( ++p == pe )
		goto _test_eof65;
case 65:
	switch( (*p) ) {
		case 13: goto tr57;
		case 73: goto st66;
		case 105: goto st66;
	}
	goto st42;
st66:
	if ( ++p == pe )
		goto _test_eof66;
case 66:
	switch( (*p) ) {
		case 13: goto tr57;
		case 86: goto st67;
		case 118: goto st67;
	}
	goto st42;
st67:
	if ( ++p == pe )
		goto _test_eof67;
case 67:
	switch( (*p) ) {
		case 13: goto tr57;
		case 69: goto st68;
		case 101: goto st68;
	}
	goto st42;
st68:
	if ( ++p == pe )
		goto _test_eof68;
case 68:
	if ( (*p) == 13 )
		goto tr86;
	goto st42;
st69:
	if ( ++p == pe )
		goto _test_eof69;
case 69:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 69: goto st70;
		case 101: goto st70;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st70:
	if ( ++p == pe )
		goto _test_eof70;
case 70:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st71;
		case 110: goto st71;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st71:
	if ( ++p == pe )
		goto _test_eof71;
case 71:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 84: goto st72;
		case 116: goto st72;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st72:
	if ( ++p == pe )
		goto _test_eof72;
case 72:
	switch( (*p) ) {
		case 33: goto st40;
		case 45: goto st73;
		case 46: goto st40;
		case 58: goto tr52;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else if ( (*p) >= 65 )
			goto st40;
	} else
		goto st40;
	goto st0;
st73:
	if ( ++p == pe )
		goto _test_eof73;
case 73:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 76: goto st74;
		case 108: goto st74;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st74:
	if ( ++p == pe )
		goto _test_eof74;
case 74:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 69: goto st75;
		case 101: goto st75;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st75:
	if ( ++p == pe )
		goto _test_eof75;
case 75:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st76;
		case 110: goto st76;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st76:
	if ( ++p == pe )
		goto _test_eof76;
case 76:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 71: goto st77;
		case 103: goto st77;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st77:
	if ( ++p == pe )
		goto _test_eof77;
case 77:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 84: goto st78;
		case 116: goto st78;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st78:
	if ( ++p == pe )
		goto _test_eof78;
case 78:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 72: goto st79;
		case 104: goto st79;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st79:
	if ( ++p == pe )
		goto _test_eof79;
case 79:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr97;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
tr97:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st80;
st80:
	if ( ++p == pe )
		goto _test_eof80;
case 80:
#line 2293 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr54;
		case 32: goto st80;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr99;
	goto tr53;
tr99:
#line 209 "http_parser.rl"
	{
    if (parser->content_length == -1) parser->content_length = 0;
    if (parser->content_length > INT_MAX) {
      parser->flags |= ERROR;
      return;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st81;
tr100:
#line 209 "http_parser.rl"
	{
    if (parser->content_length == -1) parser->content_length = 0;
    if (parser->content_length > INT_MAX) {
      parser->flags |= ERROR;
      return;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
	goto st81;
st81:
	if ( ++p == pe )
		goto _test_eof81;
case 81:
#line 2334 "http_parser.c"
	if ( (*p) == 13 )
		goto tr57;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr100;
	goto st42;
tr49:
#line 123 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st82;
st82:
	if ( ++p == pe )
		goto _test_eof82;
case 82:
#line 2351 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 82: goto st83;
		case 114: goto st83;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st83:
	if ( ++p == pe )
		goto _test_eof83;
case 83:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 65: goto st84;
		case 97: goto st84;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 66 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st84:
	if ( ++p == pe )
		goto _test_eof84;
case 84:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st85;
		case 110: goto st85;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st85:
	if ( ++p == pe )
		goto _test_eof85;
case 85:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 83: goto st86;
		case 115: goto st86;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st86:
	if ( ++p == pe )
		goto _test_eof86;
case 86:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 70: goto st87;
		case 102: goto st87;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st87:
	if ( ++p == pe )
		goto _test_eof87;
case 87:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 69: goto st88;
		case 101: goto st88;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st88:
	if ( ++p == pe )
		goto _test_eof88;
case 88:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 82: goto st89;
		case 114: goto st89;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st89:
	if ( ++p == pe )
		goto _test_eof89;
case 89:
	switch( (*p) ) {
		case 33: goto st40;
		case 45: goto st90;
		case 46: goto st40;
		case 58: goto tr52;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else if ( (*p) >= 65 )
			goto st40;
	} else
		goto st40;
	goto st0;
st90:
	if ( ++p == pe )
		goto _test_eof90;
case 90:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 69: goto st91;
		case 101: goto st91;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st91:
	if ( ++p == pe )
		goto _test_eof91;
case 91:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st92;
		case 110: goto st92;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st92:
	if ( ++p == pe )
		goto _test_eof92;
case 92:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 67: goto st93;
		case 99: goto st93;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st93:
	if ( ++p == pe )
		goto _test_eof93;
case 93:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 79: goto st94;
		case 111: goto st94;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st94:
	if ( ++p == pe )
		goto _test_eof94;
case 94:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 68: goto st95;
		case 100: goto st95;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st95:
	if ( ++p == pe )
		goto _test_eof95;
case 95:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 73: goto st96;
		case 105: goto st96;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st96:
	if ( ++p == pe )
		goto _test_eof96;
case 96:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 78: goto st97;
		case 110: goto st97;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st97:
	if ( ++p == pe )
		goto _test_eof97;
case 97:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr52;
		case 71: goto st98;
		case 103: goto st98;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
st98:
	if ( ++p == pe )
		goto _test_eof98;
case 98:
	switch( (*p) ) {
		case 33: goto st40;
		case 58: goto tr117;
		case 124: goto st40;
		case 126: goto st40;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st40;
		} else if ( (*p) >= 35 )
			goto st40;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st40;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st40;
		} else
			goto st40;
	} else
		goto st40;
	goto st0;
tr117:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st99;
st99:
	if ( ++p == pe )
		goto _test_eof99;
case 99:
#line 2865 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr54;
		case 32: goto st99;
		case 67: goto tr119;
		case 99: goto tr119;
	}
	goto tr53;
tr119:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st100;
st100:
	if ( ++p == pe )
		goto _test_eof100;
case 100:
#line 2884 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr57;
		case 72: goto st101;
		case 104: goto st101;
	}
	goto st42;
st101:
	if ( ++p == pe )
		goto _test_eof101;
case 101:
	switch( (*p) ) {
		case 13: goto tr57;
		case 85: goto st102;
		case 117: goto st102;
	}
	goto st42;
st102:
	if ( ++p == pe )
		goto _test_eof102;
case 102:
	switch( (*p) ) {
		case 13: goto tr57;
		case 78: goto st103;
		case 110: goto st103;
	}
	goto st42;
st103:
	if ( ++p == pe )
		goto _test_eof103;
case 103:
	switch( (*p) ) {
		case 13: goto tr57;
		case 75: goto st104;
		case 107: goto st104;
	}
	goto st42;
st104:
	if ( ++p == pe )
		goto _test_eof104;
case 104:
	switch( (*p) ) {
		case 13: goto tr57;
		case 69: goto st105;
		case 101: goto st105;
	}
	goto st42;
st105:
	if ( ++p == pe )
		goto _test_eof105;
case 105:
	switch( (*p) ) {
		case 13: goto tr57;
		case 68: goto st106;
		case 100: goto st106;
	}
	goto st42;
st106:
	if ( ++p == pe )
		goto _test_eof106;
case 106:
	if ( (*p) == 13 )
		goto tr126;
	goto st42;
st107:
	if ( ++p == pe )
		goto _test_eof107;
case 107:
	if ( (*p) == 13 )
		goto tr127;
	goto st0;
st108:
	if ( ++p == pe )
		goto _test_eof108;
case 108:
	if ( (*p) == 46 )
		goto st109;
	goto st0;
st109:
	if ( ++p == pe )
		goto _test_eof109;
case 109:
	switch( (*p) ) {
		case 48: goto st110;
		case 49: goto st111;
	}
	if ( 50 <= (*p) && (*p) <= 57 )
		goto st36;
	goto st0;
st110:
	if ( ++p == pe )
		goto _test_eof110;
case 110:
	if ( (*p) == 13 )
		goto tr131;
	goto st0;
st111:
	if ( ++p == pe )
		goto _test_eof111;
case 111:
	if ( (*p) == 13 )
		goto tr132;
	goto st0;
st112:
	if ( ++p == pe )
		goto _test_eof112;
case 112:
	if ( (*p) == 46 )
		goto st113;
	goto st0;
st113:
	if ( ++p == pe )
		goto _test_eof113;
case 113:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st36;
	goto st0;
tr32:
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st114;
tr147:
#line 183 "http_parser.rl"
	{
    CALLBACK(path);
    parser->path_mark = NULL;
    parser->path_size = 0;
  }
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st114;
tr153:
#line 138 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
#line 177 "http_parser.rl"
	{
    CALLBACK(query_string);
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st114;
tr157:
#line 177 "http_parser.rl"
	{
    CALLBACK(query_string);
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }
#line 165 "http_parser.rl"
	{
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }
	goto st114;
st114:
	if ( ++p == pe )
		goto _test_eof114;
case 114:
#line 3060 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr135;
		case 35: goto st0;
		case 37: goto tr136;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto tr134;
tr134:
#line 133 "http_parser.rl"
	{
    parser->fragment_mark = p;
    parser->fragment_size = 0;
  }
	goto st115;
st115:
	if ( ++p == pe )
		goto _test_eof115;
case 115:
#line 3083 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr138;
		case 35: goto st0;
		case 37: goto st116;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st115;
tr136:
#line 133 "http_parser.rl"
	{
    parser->fragment_mark = p;
    parser->fragment_size = 0;
  }
	goto st116;
st116:
	if ( ++p == pe )
		goto _test_eof116;
case 116:
#line 3106 "http_parser.c"
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st117;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st117;
	} else
		goto st117;
	goto st0;
st117:
	if ( ++p == pe )
		goto _test_eof117;
case 117:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st115;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st115;
	} else
		goto st115;
	goto st0;
tr28:
#line 148 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
	goto st118;
st118:
	if ( ++p == pe )
		goto _test_eof118;
case 118:
#line 3140 "http_parser.c"
	switch( (*p) ) {
		case 43: goto st118;
		case 58: goto st119;
	}
	if ( (*p) < 48 ) {
		if ( 45 <= (*p) && (*p) <= 46 )
			goto st118;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 97 <= (*p) && (*p) <= 122 )
				goto st118;
		} else if ( (*p) >= 65 )
			goto st118;
	} else
		goto st118;
	goto st0;
tr30:
#line 148 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
	goto st119;
st119:
	if ( ++p == pe )
		goto _test_eof119;
case 119:
#line 3168 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr31;
		case 35: goto tr32;
		case 37: goto st120;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st119;
st120:
	if ( ++p == pe )
		goto _test_eof120;
case 120:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st121;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st121;
	} else
		goto st121;
	goto st0;
st121:
	if ( ++p == pe )
		goto _test_eof121;
case 121:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st119;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st119;
	} else
		goto st119;
	goto st0;
tr29:
#line 148 "http_parser.rl"
	{
    parser->uri_mark = p;
    parser->uri_size = 0;
  }
#line 143 "http_parser.rl"
	{
    parser->path_mark = p;
    parser->path_size = 0;
  }
	goto st122;
st122:
	if ( ++p == pe )
		goto _test_eof122;
case 122:
#line 3222 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr146;
		case 35: goto tr147;
		case 37: goto st123;
		case 60: goto st0;
		case 62: goto st0;
		case 63: goto tr149;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st122;
st123:
	if ( ++p == pe )
		goto _test_eof123;
case 123:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st124;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st124;
	} else
		goto st124;
	goto st0;
st124:
	if ( ++p == pe )
		goto _test_eof124;
case 124:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st122;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st122;
	} else
		goto st122;
	goto st0;
tr149:
#line 183 "http_parser.rl"
	{
    CALLBACK(path);
    parser->path_mark = NULL;
    parser->path_size = 0;
  }
	goto st125;
st125:
	if ( ++p == pe )
		goto _test_eof125;
case 125:
#line 3273 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr152;
		case 35: goto tr153;
		case 37: goto tr154;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto tr151;
tr151:
#line 138 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
	goto st126;
st126:
	if ( ++p == pe )
		goto _test_eof126;
case 126:
#line 3296 "http_parser.c"
	switch( (*p) ) {
		case 32: goto tr156;
		case 35: goto tr157;
		case 37: goto st127;
		case 60: goto st0;
		case 62: goto st0;
		case 127: goto st0;
	}
	if ( 0 <= (*p) && (*p) <= 31 )
		goto st0;
	goto st126;
tr154:
#line 138 "http_parser.rl"
	{
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }
	goto st127;
st127:
	if ( ++p == pe )
		goto _test_eof127;
case 127:
#line 3319 "http_parser.c"
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st128;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st128;
	} else
		goto st128;
	goto st0;
st128:
	if ( ++p == pe )
		goto _test_eof128;
case 128:
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st126;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st126;
	} else
		goto st126;
	goto st0;
tr331:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st129;
st129:
	if ( ++p == pe )
		goto _test_eof129;
case 129:
#line 3358 "http_parser.c"
	if ( (*p) == 69 )
		goto st130;
	goto st0;
st130:
	if ( ++p == pe )
		goto _test_eof130;
case 130:
	if ( (*p) == 76 )
		goto st131;
	goto st0;
st131:
	if ( ++p == pe )
		goto _test_eof131;
case 131:
	if ( (*p) == 69 )
		goto st132;
	goto st0;
st132:
	if ( ++p == pe )
		goto _test_eof132;
case 132:
	if ( (*p) == 84 )
		goto st133;
	goto st0;
st133:
	if ( ++p == pe )
		goto _test_eof133;
case 133:
	if ( (*p) == 69 )
		goto st134;
	goto st0;
st134:
	if ( ++p == pe )
		goto _test_eof134;
case 134:
	if ( (*p) == 32 )
		goto tr165;
	goto st0;
tr332:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st135;
st135:
	if ( ++p == pe )
		goto _test_eof135;
case 135:
#line 3413 "http_parser.c"
	if ( (*p) == 69 )
		goto st136;
	goto st0;
st136:
	if ( ++p == pe )
		goto _test_eof136;
case 136:
	if ( (*p) == 84 )
		goto st137;
	goto st0;
st137:
	if ( ++p == pe )
		goto _test_eof137;
case 137:
	if ( (*p) == 32 )
		goto tr168;
	goto st0;
tr333:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st138;
st138:
	if ( ++p == pe )
		goto _test_eof138;
case 138:
#line 3447 "http_parser.c"
	if ( (*p) == 69 )
		goto st139;
	goto st0;
st139:
	if ( ++p == pe )
		goto _test_eof139;
case 139:
	if ( (*p) == 65 )
		goto st140;
	goto st0;
st140:
	if ( ++p == pe )
		goto _test_eof140;
case 140:
	if ( (*p) == 68 )
		goto st141;
	goto st0;
st141:
	if ( ++p == pe )
		goto _test_eof141;
case 141:
	if ( (*p) == 32 )
		goto tr172;
	goto st0;
tr334:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st142;
st142:
	if ( ++p == pe )
		goto _test_eof142;
case 142:
#line 3488 "http_parser.c"
	if ( (*p) == 79 )
		goto st143;
	goto st0;
st143:
	if ( ++p == pe )
		goto _test_eof143;
case 143:
	if ( (*p) == 67 )
		goto st144;
	goto st0;
st144:
	if ( ++p == pe )
		goto _test_eof144;
case 144:
	if ( (*p) == 75 )
		goto st145;
	goto st0;
st145:
	if ( ++p == pe )
		goto _test_eof145;
case 145:
	if ( (*p) == 32 )
		goto tr176;
	goto st0;
tr335:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st146;
st146:
	if ( ++p == pe )
		goto _test_eof146;
case 146:
#line 3529 "http_parser.c"
	switch( (*p) ) {
		case 75: goto st147;
		case 79: goto st151;
	}
	goto st0;
st147:
	if ( ++p == pe )
		goto _test_eof147;
case 147:
	if ( (*p) == 67 )
		goto st148;
	goto st0;
st148:
	if ( ++p == pe )
		goto _test_eof148;
case 148:
	if ( (*p) == 79 )
		goto st149;
	goto st0;
st149:
	if ( ++p == pe )
		goto _test_eof149;
case 149:
	if ( (*p) == 76 )
		goto st150;
	goto st0;
st150:
	if ( ++p == pe )
		goto _test_eof150;
case 150:
	if ( (*p) == 32 )
		goto tr182;
	goto st0;
st151:
	if ( ++p == pe )
		goto _test_eof151;
case 151:
	if ( (*p) == 86 )
		goto st152;
	goto st0;
st152:
	if ( ++p == pe )
		goto _test_eof152;
case 152:
	if ( (*p) == 69 )
		goto st153;
	goto st0;
st153:
	if ( ++p == pe )
		goto _test_eof153;
case 153:
	if ( (*p) == 32 )
		goto tr185;
	goto st0;
tr336:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st154;
st154:
	if ( ++p == pe )
		goto _test_eof154;
case 154:
#line 3600 "http_parser.c"
	if ( (*p) == 80 )
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
	if ( (*p) == 73 )
		goto st157;
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
	if ( (*p) == 78 )
		goto st159;
	goto st0;
st159:
	if ( ++p == pe )
		goto _test_eof159;
case 159:
	if ( (*p) == 83 )
		goto st160;
	goto st0;
st160:
	if ( ++p == pe )
		goto _test_eof160;
case 160:
	if ( (*p) == 32 )
		goto tr192;
	goto st0;
tr337:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st161;
st161:
	if ( ++p == pe )
		goto _test_eof161;
case 161:
#line 3662 "http_parser.c"
	switch( (*p) ) {
		case 79: goto st162;
		case 82: goto st165;
		case 85: goto st177;
	}
	goto st0;
st162:
	if ( ++p == pe )
		goto _test_eof162;
case 162:
	if ( (*p) == 83 )
		goto st163;
	goto st0;
st163:
	if ( ++p == pe )
		goto _test_eof163;
case 163:
	if ( (*p) == 84 )
		goto st164;
	goto st0;
st164:
	if ( ++p == pe )
		goto _test_eof164;
case 164:
	if ( (*p) == 32 )
		goto tr198;
	goto st0;
st165:
	if ( ++p == pe )
		goto _test_eof165;
case 165:
	if ( (*p) == 79 )
		goto st166;
	goto st0;
st166:
	if ( ++p == pe )
		goto _test_eof166;
case 166:
	if ( (*p) == 80 )
		goto st167;
	goto st0;
st167:
	if ( ++p == pe )
		goto _test_eof167;
case 167:
	switch( (*p) ) {
		case 70: goto st168;
		case 80: goto st172;
	}
	goto st0;
st168:
	if ( ++p == pe )
		goto _test_eof168;
case 168:
	if ( (*p) == 73 )
		goto st169;
	goto st0;
st169:
	if ( ++p == pe )
		goto _test_eof169;
case 169:
	if ( (*p) == 78 )
		goto st170;
	goto st0;
st170:
	if ( ++p == pe )
		goto _test_eof170;
case 170:
	if ( (*p) == 68 )
		goto st171;
	goto st0;
st171:
	if ( ++p == pe )
		goto _test_eof171;
case 171:
	if ( (*p) == 32 )
		goto tr206;
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
	if ( (*p) == 84 )
		goto st174;
	goto st0;
st174:
	if ( ++p == pe )
		goto _test_eof174;
case 174:
	if ( (*p) == 67 )
		goto st175;
	goto st0;
st175:
	if ( ++p == pe )
		goto _test_eof175;
case 175:
	if ( (*p) == 72 )
		goto st176;
	goto st0;
st176:
	if ( ++p == pe )
		goto _test_eof176;
case 176:
	if ( (*p) == 32 )
		goto tr211;
	goto st0;
st177:
	if ( ++p == pe )
		goto _test_eof177;
case 177:
	if ( (*p) == 84 )
		goto st178;
	goto st0;
st178:
	if ( ++p == pe )
		goto _test_eof178;
case 178:
	if ( (*p) == 32 )
		goto tr213;
	goto st0;
tr338:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st179;
st179:
	if ( ++p == pe )
		goto _test_eof179;
case 179:
#line 3806 "http_parser.c"
	if ( (*p) == 82 )
		goto st180;
	goto st0;
st180:
	if ( ++p == pe )
		goto _test_eof180;
case 180:
	if ( (*p) == 65 )
		goto st181;
	goto st0;
st181:
	if ( ++p == pe )
		goto _test_eof181;
case 181:
	if ( (*p) == 67 )
		goto st182;
	goto st0;
st182:
	if ( ++p == pe )
		goto _test_eof182;
case 182:
	if ( (*p) == 69 )
		goto st183;
	goto st0;
st183:
	if ( ++p == pe )
		goto _test_eof183;
case 183:
	if ( (*p) == 32 )
		goto tr218;
	goto st0;
tr339:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st184;
st184:
	if ( ++p == pe )
		goto _test_eof184;
case 184:
#line 3854 "http_parser.c"
	if ( (*p) == 78 )
		goto st185;
	goto st0;
st185:
	if ( ++p == pe )
		goto _test_eof185;
case 185:
	if ( (*p) == 76 )
		goto st186;
	goto st0;
st186:
	if ( ++p == pe )
		goto _test_eof186;
case 186:
	if ( (*p) == 79 )
		goto st187;
	goto st0;
st187:
	if ( ++p == pe )
		goto _test_eof187;
case 187:
	if ( (*p) == 67 )
		goto st188;
	goto st0;
st188:
	if ( ++p == pe )
		goto _test_eof188;
case 188:
	if ( (*p) == 75 )
		goto st189;
	goto st0;
st189:
	if ( ++p == pe )
		goto _test_eof189;
case 189:
	if ( (*p) == 32 )
		goto tr224;
	goto st0;
tr246:
	cs = 283;
#line 189 "http_parser.rl"
	{
    if(parser->on_headers_complete) {
      callback_return_value = parser->on_headers_complete(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
#line 262 "http_parser.rl"
	{
    if (parser->flags & CHUNKED) {
      cs = 2;
    } else {
      /* this is pretty stupid. i'd prefer to combine this with
       * skip_chunk_data */
      if (parser->content_length < 0) {
        /* If we didn't get a content length; if not keep-alive
         * just read body until EOF */
        if (!http_parser_should_keep_alive(parser)) {
          parser->flags |= EAT_FOREVER;
          parser->chunk_size = REMAINING;
        } else {
          /* Otherwise, if keep-alive, then assume the message
           * has no body. */
          parser->chunk_size = parser->content_length = 0;
        }
      } else {
        parser->chunk_size = parser->content_length;
      }
      p += 1;

      SKIP_BODY(MIN(REMAINING, parser->chunk_size));

      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }

      p--;
      if(parser->chunk_size > REMAINING) {
        {p++; goto _out;}
      }
    }
  }
	goto _again;
st283:
	if ( ++p == pe )
		goto _test_eof283;
case 283:
#line 3946 "http_parser.c"
	if ( (*p) == 72 )
		goto tr340;
	goto st0;
tr340:
#line 199 "http_parser.rl"
	{
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }
	goto st190;
st190:
	if ( ++p == pe )
		goto _test_eof190;
case 190:
#line 3966 "http_parser.c"
	if ( (*p) == 84 )
		goto st191;
	goto st0;
st191:
	if ( ++p == pe )
		goto _test_eof191;
case 191:
	if ( (*p) == 84 )
		goto st192;
	goto st0;
st192:
	if ( ++p == pe )
		goto _test_eof192;
case 192:
	if ( (*p) == 80 )
		goto st193;
	goto st0;
st193:
	if ( ++p == pe )
		goto _test_eof193;
case 193:
	if ( (*p) == 47 )
		goto st194;
	goto st0;
st194:
	if ( ++p == pe )
		goto _test_eof194;
case 194:
	switch( (*p) ) {
		case 48: goto st195;
		case 49: goto st274;
	}
	if ( 50 <= (*p) && (*p) <= 57 )
		goto st278;
	goto st0;
st195:
	if ( ++p == pe )
		goto _test_eof195;
case 195:
	if ( (*p) == 46 )
		goto st196;
	goto st0;
st196:
	if ( ++p == pe )
		goto _test_eof196;
case 196:
	if ( (*p) == 57 )
		goto st273;
	if ( 48 <= (*p) && (*p) <= 56 )
		goto st197;
	goto st0;
st197:
	if ( ++p == pe )
		goto _test_eof197;
case 197:
	if ( (*p) == 32 )
		goto st198;
	goto st0;
tr323:
#line 231 "http_parser.rl"
	{ parser->version = HTTP_VERSION_09; }
	goto st198;
tr327:
#line 230 "http_parser.rl"
	{ parser->version = HTTP_VERSION_10; }
	goto st198;
tr328:
#line 229 "http_parser.rl"
	{ parser->version = HTTP_VERSION_11; }
	goto st198;
st198:
	if ( ++p == pe )
		goto _test_eof198;
case 198:
#line 4041 "http_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr236;
	goto st0;
tr236:
#line 219 "http_parser.rl"
	{
    parser->status_code *= 10;
    parser->status_code += *p - '0';
  }
	goto st199;
st199:
	if ( ++p == pe )
		goto _test_eof199;
case 199:
#line 4056 "http_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr237;
	goto st0;
tr237:
#line 219 "http_parser.rl"
	{
    parser->status_code *= 10;
    parser->status_code += *p - '0';
  }
	goto st200;
st200:
	if ( ++p == pe )
		goto _test_eof200;
case 200:
#line 4071 "http_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr238;
	goto st0;
tr238:
#line 219 "http_parser.rl"
	{
    parser->status_code *= 10;
    parser->status_code += *p - '0';
  }
	goto st201;
st201:
	if ( ++p == pe )
		goto _test_eof201;
case 201:
#line 4086 "http_parser.c"
	switch( (*p) ) {
		case 13: goto st202;
		case 32: goto st272;
	}
	goto st0;
tr250:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st202;
tr253:
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st202;
tr272:
#line 227 "http_parser.rl"
	{ parser->keep_alive = 0; }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st202;
tr282:
#line 226 "http_parser.rl"
	{ parser->keep_alive = 1; }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st202;
tr322:
#line 224 "http_parser.rl"
	{ parser->flags |= CHUNKED;  }
#line 159 "http_parser.rl"
	{
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }
	goto st202;
st202:
	if ( ++p == pe )
		goto _test_eof202;
case 202:
#line 4147 "http_parser.c"
	if ( (*p) == 10 )
		goto st203;
	goto st0;
st203:
	if ( ++p == pe )
		goto _test_eof203;
case 203:
	switch( (*p) ) {
		case 13: goto st204;
		case 33: goto tr243;
		case 67: goto tr244;
		case 84: goto tr245;
		case 99: goto tr244;
		case 116: goto tr245;
		case 124: goto tr243;
		case 126: goto tr243;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr243;
		} else if ( (*p) >= 35 )
			goto tr243;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr243;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr243;
		} else
			goto tr243;
	} else
		goto tr243;
	goto st0;
st204:
	if ( ++p == pe )
		goto _test_eof204;
case 204:
	if ( (*p) == 10 )
		goto tr246;
	goto st0;
tr243:
#line 123 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st205;
st205:
	if ( ++p == pe )
		goto _test_eof205;
case 205:
#line 4201 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
tr248:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st206;
st206:
	if ( ++p == pe )
		goto _test_eof206;
case 206:
#line 4238 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr250;
		case 32: goto st206;
	}
	goto tr249;
tr249:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st207;
st207:
	if ( ++p == pe )
		goto _test_eof207;
case 207:
#line 4255 "http_parser.c"
	if ( (*p) == 13 )
		goto tr253;
	goto st207;
tr244:
#line 123 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st208;
st208:
	if ( ++p == pe )
		goto _test_eof208;
case 208:
#line 4270 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 79: goto st209;
		case 111: goto st209;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st209:
	if ( ++p == pe )
		goto _test_eof209;
case 209:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st210;
		case 110: goto st210;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st210:
	if ( ++p == pe )
		goto _test_eof210;
case 210:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st211;
		case 84: goto st234;
		case 110: goto st211;
		case 116: goto st234;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st211:
	if ( ++p == pe )
		goto _test_eof211;
case 211:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 69: goto st212;
		case 101: goto st212;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st212:
	if ( ++p == pe )
		goto _test_eof212;
case 212:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 67: goto st213;
		case 99: goto st213;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st213:
	if ( ++p == pe )
		goto _test_eof213;
case 213:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 84: goto st214;
		case 116: goto st214;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st214:
	if ( ++p == pe )
		goto _test_eof214;
case 214:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 73: goto st215;
		case 105: goto st215;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st215:
	if ( ++p == pe )
		goto _test_eof215;
case 215:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 79: goto st216;
		case 111: goto st216;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st216:
	if ( ++p == pe )
		goto _test_eof216;
case 216:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st217;
		case 110: goto st217;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st217:
	if ( ++p == pe )
		goto _test_eof217;
case 217:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr264;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
tr264:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st218;
st218:
	if ( ++p == pe )
		goto _test_eof218;
case 218:
#line 4579 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr250;
		case 32: goto st218;
		case 67: goto tr266;
		case 75: goto tr267;
		case 99: goto tr266;
		case 107: goto tr267;
	}
	goto tr249;
tr266:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st219;
st219:
	if ( ++p == pe )
		goto _test_eof219;
case 219:
#line 4600 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr253;
		case 76: goto st220;
		case 108: goto st220;
	}
	goto st207;
st220:
	if ( ++p == pe )
		goto _test_eof220;
case 220:
	switch( (*p) ) {
		case 13: goto tr253;
		case 79: goto st221;
		case 111: goto st221;
	}
	goto st207;
st221:
	if ( ++p == pe )
		goto _test_eof221;
case 221:
	switch( (*p) ) {
		case 13: goto tr253;
		case 83: goto st222;
		case 115: goto st222;
	}
	goto st207;
st222:
	if ( ++p == pe )
		goto _test_eof222;
case 222:
	switch( (*p) ) {
		case 13: goto tr253;
		case 69: goto st223;
		case 101: goto st223;
	}
	goto st207;
st223:
	if ( ++p == pe )
		goto _test_eof223;
case 223:
	if ( (*p) == 13 )
		goto tr272;
	goto st207;
tr267:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st224;
st224:
	if ( ++p == pe )
		goto _test_eof224;
case 224:
#line 4655 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr253;
		case 69: goto st225;
		case 101: goto st225;
	}
	goto st207;
st225:
	if ( ++p == pe )
		goto _test_eof225;
case 225:
	switch( (*p) ) {
		case 13: goto tr253;
		case 69: goto st226;
		case 101: goto st226;
	}
	goto st207;
st226:
	if ( ++p == pe )
		goto _test_eof226;
case 226:
	switch( (*p) ) {
		case 13: goto tr253;
		case 80: goto st227;
		case 112: goto st227;
	}
	goto st207;
st227:
	if ( ++p == pe )
		goto _test_eof227;
case 227:
	switch( (*p) ) {
		case 13: goto tr253;
		case 45: goto st228;
	}
	goto st207;
st228:
	if ( ++p == pe )
		goto _test_eof228;
case 228:
	switch( (*p) ) {
		case 13: goto tr253;
		case 65: goto st229;
		case 97: goto st229;
	}
	goto st207;
st229:
	if ( ++p == pe )
		goto _test_eof229;
case 229:
	switch( (*p) ) {
		case 13: goto tr253;
		case 76: goto st230;
		case 108: goto st230;
	}
	goto st207;
st230:
	if ( ++p == pe )
		goto _test_eof230;
case 230:
	switch( (*p) ) {
		case 13: goto tr253;
		case 73: goto st231;
		case 105: goto st231;
	}
	goto st207;
st231:
	if ( ++p == pe )
		goto _test_eof231;
case 231:
	switch( (*p) ) {
		case 13: goto tr253;
		case 86: goto st232;
		case 118: goto st232;
	}
	goto st207;
st232:
	if ( ++p == pe )
		goto _test_eof232;
case 232:
	switch( (*p) ) {
		case 13: goto tr253;
		case 69: goto st233;
		case 101: goto st233;
	}
	goto st207;
st233:
	if ( ++p == pe )
		goto _test_eof233;
case 233:
	if ( (*p) == 13 )
		goto tr282;
	goto st207;
st234:
	if ( ++p == pe )
		goto _test_eof234;
case 234:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 69: goto st235;
		case 101: goto st235;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st235:
	if ( ++p == pe )
		goto _test_eof235;
case 235:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st236;
		case 110: goto st236;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st236:
	if ( ++p == pe )
		goto _test_eof236;
case 236:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 84: goto st237;
		case 116: goto st237;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st237:
	if ( ++p == pe )
		goto _test_eof237;
case 237:
	switch( (*p) ) {
		case 33: goto st205;
		case 45: goto st238;
		case 46: goto st205;
		case 58: goto tr248;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else if ( (*p) >= 65 )
			goto st205;
	} else
		goto st205;
	goto st0;
st238:
	if ( ++p == pe )
		goto _test_eof238;
case 238:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 76: goto st239;
		case 108: goto st239;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st239:
	if ( ++p == pe )
		goto _test_eof239;
case 239:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 69: goto st240;
		case 101: goto st240;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st240:
	if ( ++p == pe )
		goto _test_eof240;
case 240:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st241;
		case 110: goto st241;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st241:
	if ( ++p == pe )
		goto _test_eof241;
case 241:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 71: goto st242;
		case 103: goto st242;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st242:
	if ( ++p == pe )
		goto _test_eof242;
case 242:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 84: goto st243;
		case 116: goto st243;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st243:
	if ( ++p == pe )
		goto _test_eof243;
case 243:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 72: goto st244;
		case 104: goto st244;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st244:
	if ( ++p == pe )
		goto _test_eof244;
case 244:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr293;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
tr293:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st245;
st245:
	if ( ++p == pe )
		goto _test_eof245;
case 245:
#line 5085 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr250;
		case 32: goto st245;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr295;
	goto tr249;
tr295:
#line 209 "http_parser.rl"
	{
    if (parser->content_length == -1) parser->content_length = 0;
    if (parser->content_length > INT_MAX) {
      parser->flags |= ERROR;
      return;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st246;
tr296:
#line 209 "http_parser.rl"
	{
    if (parser->content_length == -1) parser->content_length = 0;
    if (parser->content_length > INT_MAX) {
      parser->flags |= ERROR;
      return;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }
	goto st246;
st246:
	if ( ++p == pe )
		goto _test_eof246;
case 246:
#line 5126 "http_parser.c"
	if ( (*p) == 13 )
		goto tr253;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr296;
	goto st207;
tr245:
#line 123 "http_parser.rl"
	{
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }
	goto st247;
st247:
	if ( ++p == pe )
		goto _test_eof247;
case 247:
#line 5143 "http_parser.c"
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 82: goto st248;
		case 114: goto st248;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st248:
	if ( ++p == pe )
		goto _test_eof248;
case 248:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 65: goto st249;
		case 97: goto st249;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 66 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st249:
	if ( ++p == pe )
		goto _test_eof249;
case 249:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st250;
		case 110: goto st250;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st250:
	if ( ++p == pe )
		goto _test_eof250;
case 250:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 83: goto st251;
		case 115: goto st251;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st251:
	if ( ++p == pe )
		goto _test_eof251;
case 251:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 70: goto st252;
		case 102: goto st252;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st252:
	if ( ++p == pe )
		goto _test_eof252;
case 252:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 69: goto st253;
		case 101: goto st253;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st253:
	if ( ++p == pe )
		goto _test_eof253;
case 253:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 82: goto st254;
		case 114: goto st254;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st254:
	if ( ++p == pe )
		goto _test_eof254;
case 254:
	switch( (*p) ) {
		case 33: goto st205;
		case 45: goto st255;
		case 46: goto st205;
		case 58: goto tr248;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 48 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else if ( (*p) >= 65 )
			goto st205;
	} else
		goto st205;
	goto st0;
st255:
	if ( ++p == pe )
		goto _test_eof255;
case 255:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 69: goto st256;
		case 101: goto st256;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st256:
	if ( ++p == pe )
		goto _test_eof256;
case 256:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st257;
		case 110: goto st257;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st257:
	if ( ++p == pe )
		goto _test_eof257;
case 257:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 67: goto st258;
		case 99: goto st258;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st258:
	if ( ++p == pe )
		goto _test_eof258;
case 258:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 79: goto st259;
		case 111: goto st259;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st259:
	if ( ++p == pe )
		goto _test_eof259;
case 259:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 68: goto st260;
		case 100: goto st260;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st260:
	if ( ++p == pe )
		goto _test_eof260;
case 260:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 73: goto st261;
		case 105: goto st261;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st261:
	if ( ++p == pe )
		goto _test_eof261;
case 261:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 78: goto st262;
		case 110: goto st262;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st262:
	if ( ++p == pe )
		goto _test_eof262;
case 262:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr248;
		case 71: goto st263;
		case 103: goto st263;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
st263:
	if ( ++p == pe )
		goto _test_eof263;
case 263:
	switch( (*p) ) {
		case 33: goto st205;
		case 58: goto tr313;
		case 124: goto st205;
		case 126: goto st205;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st205;
		} else if ( (*p) >= 35 )
			goto st205;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st205;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st205;
		} else
			goto st205;
	} else
		goto st205;
	goto st0;
tr313:
#line 153 "http_parser.rl"
	{
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }
	goto st264;
st264:
	if ( ++p == pe )
		goto _test_eof264;
case 264:
#line 5657 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr250;
		case 32: goto st264;
		case 67: goto tr315;
		case 99: goto tr315;
	}
	goto tr249;
tr315:
#line 128 "http_parser.rl"
	{
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }
	goto st265;
st265:
	if ( ++p == pe )
		goto _test_eof265;
case 265:
#line 5676 "http_parser.c"
	switch( (*p) ) {
		case 13: goto tr253;
		case 72: goto st266;
		case 104: goto st266;
	}
	goto st207;
st266:
	if ( ++p == pe )
		goto _test_eof266;
case 266:
	switch( (*p) ) {
		case 13: goto tr253;
		case 85: goto st267;
		case 117: goto st267;
	}
	goto st207;
st267:
	if ( ++p == pe )
		goto _test_eof267;
case 267:
	switch( (*p) ) {
		case 13: goto tr253;
		case 78: goto st268;
		case 110: goto st268;
	}
	goto st207;
st268:
	if ( ++p == pe )
		goto _test_eof268;
case 268:
	switch( (*p) ) {
		case 13: goto tr253;
		case 75: goto st269;
		case 107: goto st269;
	}
	goto st207;
st269:
	if ( ++p == pe )
		goto _test_eof269;
case 269:
	switch( (*p) ) {
		case 13: goto tr253;
		case 69: goto st270;
		case 101: goto st270;
	}
	goto st207;
st270:
	if ( ++p == pe )
		goto _test_eof270;
case 270:
	switch( (*p) ) {
		case 13: goto tr253;
		case 68: goto st271;
		case 100: goto st271;
	}
	goto st207;
st271:
	if ( ++p == pe )
		goto _test_eof271;
case 271:
	if ( (*p) == 13 )
		goto tr322;
	goto st207;
st272:
	if ( ++p == pe )
		goto _test_eof272;
case 272:
	if ( (*p) == 13 )
		goto st202;
	if ( (*p) > 9 ) {
		if ( 11 <= (*p) )
			goto st272;
	} else if ( (*p) >= 0 )
		goto st272;
	goto st0;
st273:
	if ( ++p == pe )
		goto _test_eof273;
case 273:
	if ( (*p) == 32 )
		goto tr323;
	goto st0;
st274:
	if ( ++p == pe )
		goto _test_eof274;
case 274:
	if ( (*p) == 46 )
		goto st275;
	goto st0;
st275:
	if ( ++p == pe )
		goto _test_eof275;
case 275:
	switch( (*p) ) {
		case 48: goto st276;
		case 49: goto st277;
	}
	if ( 50 <= (*p) && (*p) <= 57 )
		goto st197;
	goto st0;
st276:
	if ( ++p == pe )
		goto _test_eof276;
case 276:
	if ( (*p) == 32 )
		goto tr327;
	goto st0;
st277:
	if ( ++p == pe )
		goto _test_eof277;
case 277:
	if ( (*p) == 32 )
		goto tr328;
	goto st0;
st278:
	if ( ++p == pe )
		goto _test_eof278;
case 278:
	if ( (*p) == 46 )
		goto st279;
	goto st0;
st279:
	if ( ++p == pe )
		goto _test_eof279;
case 279:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st197;
	goto st0;
	}
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof280: cs = 280; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof281: cs = 281; goto _test_eof; 
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
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof282: cs = 282; goto _test_eof; 
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
	_test_eof182: cs = 182; goto _test_eof; 
	_test_eof183: cs = 183; goto _test_eof; 
	_test_eof184: cs = 184; goto _test_eof; 
	_test_eof185: cs = 185; goto _test_eof; 
	_test_eof186: cs = 186; goto _test_eof; 
	_test_eof187: cs = 187; goto _test_eof; 
	_test_eof188: cs = 188; goto _test_eof; 
	_test_eof189: cs = 189; goto _test_eof; 
	_test_eof283: cs = 283; goto _test_eof; 
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
	_test_eof266: cs = 266; goto _test_eof; 
	_test_eof267: cs = 267; goto _test_eof; 
	_test_eof268: cs = 268; goto _test_eof; 
	_test_eof269: cs = 269; goto _test_eof; 
	_test_eof270: cs = 270; goto _test_eof; 
	_test_eof271: cs = 271; goto _test_eof; 
	_test_eof272: cs = 272; goto _test_eof; 
	_test_eof273: cs = 273; goto _test_eof; 
	_test_eof274: cs = 274; goto _test_eof; 
	_test_eof275: cs = 275; goto _test_eof; 
	_test_eof276: cs = 276; goto _test_eof; 
	_test_eof277: cs = 277; goto _test_eof; 
	_test_eof278: cs = 278; goto _test_eof; 
	_test_eof279: cs = 279; goto _test_eof; 

	_test_eof: {}
	_out: {}
	}
#line 482 "http_parser.rl"

  parser->cs = cs;

  CALLBACK(header_field);
  CALLBACK(header_value);
  CALLBACK(fragment);
  CALLBACK(query_string);
  CALLBACK(path);
  CALLBACK(uri);

  assert(p <= pe && "buffer overflow after parsing execute");
}

int
http_parser_has_error (http_parser *parser)
{
  if (parser->flags & ERROR) return 1;
  return parser->cs == http_parser_error;
}
