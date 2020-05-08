#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "test.h"
#include "testutil.h"

int test_empty(void) {
	check(parse("{}", 1, 1,
				JSMN_OBJECT, 0, 2, 0));
	check(parse("[]", 1, 1,
				JSMN_ARRAY, 0, 2, 0));
	check(parse("[{},{}]", 3, 3,
				JSMN_ARRAY, 0, 7, 2,
				JSMN_OBJECT, 1, 3, 0,
				JSMN_OBJECT, 4, 6, 0));
	return 0;
}

int test_object(void) {
	check(parse("{\"a\":0}", 3, 3,
				JSMN_OBJECT, 0, 7, 1,
				JSMN_STRING, "a", 1,
				JSMN_PRIMITIVE, "0"));
	check(parse("{\"a\":[]}", 3, 3,
				JSMN_OBJECT, 0, 8, 1,
				JSMN_STRING, "a", 1,
				JSMN_ARRAY, 5, 7, 0));
	check(parse("{\"a\":{},\"b\":{}}", 5, 5,
				JSMN_OBJECT, -1, -1, 2,
				JSMN_STRING, "a", 1,
				JSMN_OBJECT, -1, -1, 0,
				JSMN_STRING, "b", 1,
				JSMN_OBJECT, -1, -1, 0));
	check(parse("{\n \"Day\": 26,\n \"Month\": 9,\n \"Year\": 12\n }", 7, 7,
				JSMN_OBJECT, -1, -1, 3,
				JSMN_STRING, "Day", 1,
				JSMN_PRIMITIVE, "26",
				JSMN_STRING, "Month", 1,
				JSMN_PRIMITIVE, "9",
				JSMN_STRING, "Year", 1,
				JSMN_PRIMITIVE, "12"));
	check(parse("{\"a\": 0, \"b\": \"c\"}", 5, 5,
				JSMN_OBJECT, -1, -1, 2,
				JSMN_STRING, "a", 1,
				JSMN_PRIMITIVE, "0",
				JSMN_STRING, "b", 1,
				JSMN_STRING, "c", 0));

#ifdef JSMN_STRICT
	check(parse("{\"a\"\n0}", JSMN_ERROR_INVAL, 3));
	check(parse("{\"a\", 0}", JSMN_ERROR_INVAL, 3));
	check(parse("{\"a\": {2}}", JSMN_ERROR_INVAL, 3));
	check(parse("{\"a\": {2: 3}}", JSMN_ERROR_INVAL, 3));
	check(parse("{\"a\": {\"a\": 2 3}}", JSMN_ERROR_INVAL, 5));
	/* FIXME */
	/*check(parse("{\"a\"}", JSMN_ERROR_INVAL, 2));*/
	/*check(parse("{\"a\": 1, \"b\"}", JSMN_ERROR_INVAL, 4));*/
	/*check(parse("{\"a\",\"b\":1}", JSMN_ERROR_INVAL, 4));*/
	/*check(parse("{\"a\":1,}", JSMN_ERROR_INVAL, 4));*/
	/*check(parse("{\"a\":\"b\":\"c\"}", JSMN_ERROR_INVAL, 4));*/
	/*check(parse("{,}", JSMN_ERROR_INVAL, 4));*/
#endif
	return 0;
}

int test_array(void) {
	/* FIXME */
	/*check(parse("[10}", JSMN_ERROR_INVAL, 3));*/
	/*check(parse("[1,,3]", JSMN_ERROR_INVAL, 3)*/
	check(parse("[10]", 2, 2,
				JSMN_ARRAY, -1, -1, 1,
				JSMN_PRIMITIVE, "10"));
	check(parse("{\"a\": 1]", JSMN_ERROR_INVAL, 3));
	/* FIXME */
	/*check(parse("[\"a\": 1]", JSMN_ERROR_INVAL, 3));*/
	return 0;
}

int test_primitive(void) {
	check(parse("{\"boolVar\" : true }", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "boolVar", 1,
				JSMN_PRIMITIVE, "true"));
	check(parse("{\"boolVar\" : false }", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "boolVar", 1,
				JSMN_PRIMITIVE, "false"));
	check(parse("{\"nullVar\" : null }", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "nullVar", 1,
				JSMN_PRIMITIVE, "null"));
	check(parse("{\"intVar\" : 12}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "intVar", 1,
				JSMN_PRIMITIVE, "12"));
	check(parse("{\"floatVar\" : 12.345}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "floatVar", 1,
				JSMN_PRIMITIVE, "12.345"));
	return 0;
}

int test_string(void) {
	check(parse("{\"strVar\" : \"hello world\"}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "strVar", 1,
				JSMN_STRING, "hello world", 0));
	check(parse("{\"strVar\" : \"escapes: \\/\\r\\n\\t\\b\\f\\\"\\\\\"}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "strVar", 1,
				JSMN_STRING, "escapes: \\/\\r\\n\\t\\b\\f\\\"\\\\", 0));
	check(parse("{\"strVar\": \"\"}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "strVar", 1,
				JSMN_STRING, "", 0));
	check(parse("{\"a\":\"\\uAbcD\"}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "a", 1,
				JSMN_STRING, "\\uAbcD", 0));
	check(parse("{\"a\":\"str\\u0000\"}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "a", 1,
				JSMN_STRING, "str\\u0000", 0));
	check(parse("{\"a\":\"\\uFFFFstr\"}", 3, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "a", 1,
				JSMN_STRING, "\\uFFFFstr", 0));
	check(parse("{\"a\":[\"\\u0280\"]}", 4, 4,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "a", 1,
				JSMN_ARRAY, -1, -1, 1,
				JSMN_STRING, "\\u0280", 0));

	check(parse("{\"a\":\"str\\uFFGFstr\"}", JSMN_ERROR_INVAL, 3));
	check(parse("{\"a\":\"str\\u@FfF\"}", JSMN_ERROR_INVAL, 3));
	check(parse("{{\"a\":[\"\\u028\"]}", JSMN_ERROR_INVAL, 4));
	return 0;
}

int test_partial_string(void) {
	int i;
	int r;
	jsmn_parser p;
	jsmntok_t tok[5];
	const char *js = "{\"x\": \"va\\\\ue\", \"y\": \"value y\"}";

	jsmn_init(&p);
	for (i = 1; i <= strlen(js); i++) {
		r = jsmn_parse(&p, js, i, tok, sizeof(tok)/sizeof(tok[0]));
		if (i == strlen(js)) {
			check(r == 5);
			check(tokeq(js, tok, 5,
					JSMN_OBJECT, -1, -1, 2,
					JSMN_STRING, "x", 1,
					JSMN_STRING, "va\\\\ue", 0,
					JSMN_STRING, "y", 1,
					JSMN_STRING, "value y", 0));
		} else {
			check(r == JSMN_ERROR_PART);
		}
	}
	return 0;
}

int test_partial_array(void) {
#ifdef JSMN_STRICT
	int r;
	int i;
	jsmn_parser p;
	jsmntok_t tok[10];
	const char *js = "[ 1, true, [123, \"hello\"]]";

	jsmn_init(&p);
	for (i = 1; i <= strlen(js); i++) {
		r = jsmn_parse(&p, js, i, tok, sizeof(tok)/sizeof(tok[0]));
		if (i == strlen(js)) {
			check(r == 6);
			check(tokeq(js, tok, 6,
					JSMN_ARRAY, -1, -1, 3,
					JSMN_PRIMITIVE, "1",
					JSMN_PRIMITIVE, "true",
					JSMN_ARRAY, -1, -1, 2,
					JSMN_PRIMITIVE, "123",
					JSMN_STRING, "hello", 0));
		} else {
			check(r == JSMN_ERROR_PART);
		}
	}
#endif
	return 0;
}

int test_array_nomem(void) {
	int i;
	int r;
	jsmn_parser p;
	jsmntok_t toksmall[10], toklarge[10];
	const char *js;

	js = "  [ 1, true, [123, \"hello\"]]";

	for (i = 0; i < 6; i++) {
		jsmn_init(&p);
		memset(toksmall, 0, sizeof(toksmall));
		memset(toklarge, 0, sizeof(toklarge));
		r = jsmn_parse(&p, js, strlen(js), toksmall, i);
		check(r == JSMN_ERROR_NOMEM);

		memcpy(toklarge, toksmall, sizeof(toksmall));

		r = jsmn_parse(&p, js, strlen(js), toklarge, 10);
		check(r >= 0);
		check(tokeq(js, toklarge, 4,
					JSMN_ARRAY, -1, -1, 3,
					JSMN_PRIMITIVE, "1",
					JSMN_PRIMITIVE, "true",
					JSMN_ARRAY, -1, -1, 2,
					JSMN_PRIMITIVE, "123",
					JSMN_STRING, "hello", 0));
	}
	return 0;
}

int test_unquoted_keys(void) {
#ifndef JSMN_STRICT
	int r;
	jsmn_parser p;
	jsmntok_t tok[10];
	const char *js;

	jsmn_init(&p);
	js = "key1: \"value\"\nkey2 : 123";

	r = jsmn_parse(&p, js, strlen(js), tok, 10);
	check(r >= 0);
	check(tokeq(js, tok, 4,
				JSMN_PRIMITIVE, "key1",
				JSMN_STRING, "value", 0,
				JSMN_PRIMITIVE, "key2",
				JSMN_PRIMITIVE, "123"));
#endif
	return 0;
}

int test_issue_22(void) {
	int r;
	jsmn_parser p;
	jsmntok_t tokens[128];
	const char *js;

	js = "{ \"height\":10, \"layers\":[ { \"data\":[6,6], \"height\":10, "
		"\"name\":\"Calque de Tile 1\", \"opacity\":1, \"type\":\"tilelayer\", "
		"\"visible\":true, \"width\":10, \"x\":0, \"y\":0 }], "
		"\"orientation\":\"orthogonal\", \"properties\": { }, \"tileheight\":32, "
		"\"tilesets\":[ { \"firstgid\":1, \"image\":\"..\\/images\\/tiles.png\", "
		"\"imageheight\":64, \"imagewidth\":160, \"margin\":0, \"name\":\"Tiles\", "
		"\"properties\":{}, \"spacing\":0, \"tileheight\":32, \"tilewidth\":32 }], "
		"\"tilewidth\":32, \"version\":1, \"width\":10 }";
	jsmn_init(&p);
	r = jsmn_parse(&p, js, strlen(js), tokens, 128);
	check(r >= 0);
	return 0;
}

int test_issue_27(void) {
	const char *js =
		"{ \"name\" : \"Jack\", \"age\" : 27 } { \"name\" : \"Anna\", ";
	check(parse(js, JSMN_ERROR_PART, 8));
	return 0;
}

int test_input_length(void) {
	const char *js;
	int r;
	jsmn_parser p;
	jsmntok_t tokens[10];

	js = "{\"a\": 0}garbage";

	jsmn_init(&p);
	r = jsmn_parse(&p, js, 8, tokens, 10);
	check(r == 3);
	check(tokeq(js, tokens, 3,
				JSMN_OBJECT, -1, -1, 1,
				JSMN_STRING, "a", 1,
				JSMN_PRIMITIVE, "0"));
	return 0;
}

int test_count(void) {
	jsmn_parser p;
	const char *js;

	js = "{}";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 1);

	js = "[]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 1);

	js = "[[]]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 2);

	js = "[[], []]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 3);

	js = "[[], []]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 3);

	js = "[[], [[]], [[], []]]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 7);

	js = "[\"a\", [[], []]]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 5);

	js = "[[], \"[], [[]]\", [[]]]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 5);

	js = "[1, 2, 3]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 4);

	js = "[1, 2, [3, \"a\"], null]";
	jsmn_init(&p);
	check(jsmn_parse(&p, js, strlen(js), NULL, 0) == 7);

	return 0;
}


int test_nonstrict(void) {
#ifndef JSMN_STRICT
	const char *js;
	js = "a: 0garbage";
	check(parse(js, 2, 2,
				JSMN_PRIMITIVE, "a",
				JSMN_PRIMITIVE, "0garbage"));

	js = "Day : 26\nMonth : Sep\n\nYear: 12";
	check(parse(js, 6, 6,
				JSMN_PRIMITIVE, "Day",
				JSMN_PRIMITIVE, "26",
				JSMN_PRIMITIVE, "Month",
				JSMN_PRIMITIVE, "Sep",
				JSMN_PRIMITIVE, "Year",
				JSMN_PRIMITIVE, "12"));

	//nested {s don't cause a parse error.
	js = "\"key {1\": 1234";
	check(parse(js, 2, 2,
		              JSMN_STRING, "key {1", 1,
		              JSMN_PRIMITIVE, "1234"));


#endif
	return 0;
}

int test_unmatched_brackets(void) {
	const char *js;
	js = "\"key 1\": 1234}";
	check(parse(js, JSMN_ERROR_INVAL, 2));
	js = "{\"key 1\": 1234";
	check(parse(js, JSMN_ERROR_PART, 3));
	js = "{\"key 1\": 1234}}";
	check(parse(js, JSMN_ERROR_INVAL, 3));
	js = "\"key 1\"}: 1234";
	check(parse(js, JSMN_ERROR_INVAL, 3));
	js = "{\"key {1\": 1234}";
	check(parse(js, 3, 3,
				JSMN_OBJECT, 0, 16, 1,
				JSMN_STRING, "key {1", 1,
				JSMN_PRIMITIVE, "1234"));
	js = "{{\"key 1\": 1234}";
	check(parse(js, JSMN_ERROR_PART, 4));
	return 0;
}

int main(void) {
	test(test_empty, "test for a empty JSON objects/arrays");
	test(test_object, "test for a JSON objects");
	test(test_array, "test for a JSON arrays");
	test(test_primitive, "test primitive JSON data types");
	test(test_string, "test string JSON data types");

	test(test_partial_string, "test partial JSON string parsing");
	test(test_partial_array, "test partial array reading");
	test(test_array_nomem, "test array reading with a smaller number of tokens");
	test(test_unquoted_keys, "test unquoted keys (like in JavaScript)");
	test(test_input_length, "test strings that are not null-terminated");
	test(test_issue_22, "test issue #22");
	test(test_issue_27, "test issue #27");
	test(test_count, "test tokens count estimation");
	test(test_nonstrict, "test for non-strict mode");
	test(test_unmatched_brackets, "test for unmatched brackets");
	printf("\nPASSED: %d\nFAILED: %d\n", test_passed, test_failed);
	return (test_failed > 0);
}
