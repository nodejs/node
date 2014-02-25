// Copyright (C) 2013 - Will Glozer.  All rights reserved.

#include <stdlib.h>
#include <string.h>
#include "script.h"
#include "http_parser.h"

typedef struct {
    char *name;
    int   type;
    void *value;
} table_field;

static int script_stats_len(lua_State *);
static int script_stats_get(lua_State *);
static void set_fields(lua_State *, int index, const table_field *);

static const struct luaL_reg statslib[] = {
    { "__index", script_stats_get },
    { "__len",   script_stats_len },
    { NULL,      NULL             }
};

lua_State *script_create(char *scheme, char *host, char *port, char *path) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L, "wrk = require \"wrk\"");

    luaL_newmetatable(L, "wrk.stats");
    luaL_register(L, NULL, statslib);
    lua_pop(L, 1);

    const table_field fields[] = {
        { "scheme", LUA_TSTRING, scheme },
        { "host",   LUA_TSTRING, host   },
        { "port",   LUA_TSTRING, port   },
        { "path",   LUA_TSTRING, path   },
        { NULL,     0,           NULL   },
    };

    lua_getglobal(L, "wrk");
    set_fields(L, 1, fields);
    lua_pop(L, 1);

    return L;
}

void script_headers(lua_State *L, char **headers) {
    lua_getglobal(L, "wrk");
    lua_getfield(L, 1, "headers");
    for (char **h = headers; *h; h++) {
        char *p = strchr(*h, ':');
        if (p && p[1] == ' ') {
            lua_pushlstring(L, *h, p - *h);
            lua_pushstring(L, p + 2);
            lua_settable(L, 2);
        }
    }
    lua_pop(L, 2);
}

void script_init(lua_State *L, char *script, int argc, char **argv) {
    if (script && luaL_dofile(L, script)) {
        const char *cause = lua_tostring(L, -1);
        fprintf(stderr, "%s: %s\n", script, cause);
    }

    lua_getglobal(L, "init");
    lua_newtable(L);
    for (int i = 0; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, 2, i);
    }
    lua_call(L, 1, 0);
}

void script_request(lua_State *L, char **buf, size_t *len) {
    lua_getglobal(L, "request");
    lua_call(L, 0, 1);
    *buf = (char *) lua_tolstring(L, 1, len);
    lua_pop(L, 1);
}

void script_response(lua_State *L, int status, buffer *headers, buffer *body) {
    lua_getglobal(L, "response");
    lua_pushinteger(L, status);
    lua_newtable(L);

    for (char *c = headers->buffer; c < headers->cursor; ) {
        c = buffer_pushlstring(L, c);
        c = buffer_pushlstring(L, c);
        lua_rawset(L, -3);
    }

    lua_pushlstring(L, body->buffer, body->cursor - body->buffer);
    lua_call(L, 3, 0);

    buffer_reset(headers);
    buffer_reset(body);
}

bool script_is_static(lua_State *L) {
    lua_getglobal(L, "wrk");
    lua_getfield(L, 1, "request");
    lua_getglobal(L, "request");
    bool is_static = lua_equal(L, 2, 3);
    lua_pop(L, 3);
    return is_static;
}

bool script_want_response(lua_State *L) {
    lua_getglobal(L, "response");
    bool defined = lua_type(L, 1) == LUA_TFUNCTION;
    lua_pop(L, 1);
    return defined;
}

bool script_has_done(lua_State *L) {
    lua_getglobal(L, "done");
    bool defined = lua_type(L, 1) == LUA_TFUNCTION;
    lua_pop(L, 1);
    return defined;
}

void script_header_done(lua_State *L, luaL_Buffer *buffer) {
    luaL_pushresult(buffer);
}

void script_summary(lua_State *L, uint64_t duration, uint64_t requests, uint64_t bytes) {
    const table_field fields[] = {
        { "duration", LUA_TNUMBER, &duration },
        { "requests", LUA_TNUMBER, &requests },
        { "bytes",    LUA_TNUMBER, &bytes    },
        { NULL,       0,           NULL      },
    };
    lua_newtable(L);
    set_fields(L, 1, fields);
}

void script_errors(lua_State *L, errors *errors) {
    uint64_t e[] = {
        errors->connect,
        errors->read,
        errors->write,
        errors->status,
        errors->timeout
    };
    const table_field fields[] = {
        { "connect", LUA_TNUMBER, &e[0] },
        { "read",    LUA_TNUMBER, &e[1] },
        { "write",   LUA_TNUMBER, &e[2] },
        { "status",  LUA_TNUMBER, &e[3] },
        { "timeout", LUA_TNUMBER, &e[4] },
        { NULL,      0,           NULL  },
    };
    lua_newtable(L);
    set_fields(L, 2, fields);
    lua_setfield(L, 1, "errors");
}

void script_done(lua_State *L, stats *latency, stats *requests) {
    stats **s;

    lua_getglobal(L, "done");
    lua_pushvalue(L, 1);

    s = (stats **) lua_newuserdata(L, sizeof(stats **));
    *s = latency;
    luaL_getmetatable(L, "wrk.stats");
    lua_setmetatable(L, 4);

    s = (stats **) lua_newuserdata(L, sizeof(stats **));
    *s = requests;
    luaL_getmetatable(L, "wrk.stats");
    lua_setmetatable(L, 5);

    lua_call(L, 3, 0);
    lua_pop(L, 1);
}

static int verify_request(http_parser *parser) {
    size_t *count = parser->data;
    (*count)++;
    return 0;
}

size_t script_verify_request(lua_State *L) {
    http_parser_settings settings = {
        .on_message_complete = verify_request
    };
    http_parser parser;
    char *request;
    size_t len, count = 0;

    script_request(L, &request, &len);
    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = &count;

    size_t parsed = http_parser_execute(&parser, &settings, request, len);

    if (parsed != len || count == 0) {
        enum http_errno err = HTTP_PARSER_ERRNO(&parser);
        const char *desc = http_errno_description(err);
        const char *msg = err != HPE_OK ? desc : "incomplete request";
        int line = 1, column = 1;

        for (char *c = request; c < request + parsed; c++) {
            column++;
            if (*c == '\n') {
                column = 1;
                line++;
            }
        }

        fprintf(stderr, "%s at %d:%d\n", msg, line, column);
        exit(1);
    }

    return count;
}

static stats *checkstats(lua_State *L) {
    stats **s = luaL_checkudata(L, 1, "wrk.stats");
    luaL_argcheck(L, s != NULL, 1, "`stats' expected");
    return *s;
}

static int script_stats_percentile(lua_State *L) {
    stats *s = checkstats(L);
    lua_Number p = luaL_checknumber(L, 2);
    lua_pushnumber(L, stats_percentile(s, p));
    return 1;
}

static int script_stats_get(lua_State *L) {
    stats *s = checkstats(L);
    if (lua_isnumber(L, 2)) {
        int index = luaL_checkint(L, 2);
        lua_pushnumber(L, s->data[index - 1]);
    } else if (lua_isstring(L, 2)) {
        const char *method = lua_tostring(L, 2);
        if (!strcmp("min",   method)) lua_pushnumber(L, s->min);
        if (!strcmp("max",   method)) lua_pushnumber(L, s->max);
        if (!strcmp("mean",  method)) lua_pushnumber(L, stats_mean(s));
        if (!strcmp("stdev", method)) lua_pushnumber(L, stats_stdev(s, stats_mean(s)));
        if (!strcmp("percentile", method)) {
            lua_pushcfunction(L, script_stats_percentile);
        }
    }
    return 1;
}

static int script_stats_len(lua_State *L) {
    stats *s = checkstats(L);
    lua_pushinteger(L, s->limit);
    return 1;
}

static void set_fields(lua_State *L, int index, const table_field *fields) {
    for (int i = 0; fields[i].name; i++) {
        table_field f = fields[i];
        switch (f.value == NULL ? LUA_TNIL : f.type) {
            case LUA_TNUMBER:
                lua_pushinteger(L, *((lua_Integer *) f.value));
                break;
            case LUA_TSTRING:
                lua_pushstring(L, (const char *) f.value);
                break;
            case LUA_TNIL:
                lua_pushnil(L);
                break;
        }
        lua_setfield(L, index, f.name);
    }
}

void buffer_append(buffer *b, const char *data, size_t len) {
    size_t used = b->cursor - b->buffer;
    while (used + len + 1 >= b->length) {
        b->length += 1024;
        b->buffer  = realloc(b->buffer, b->length);
        b->cursor  = b->buffer + used;
    }
    memcpy(b->cursor, data, len);
    b->cursor += len;
}

void buffer_reset(buffer *b) {
    b->cursor = b->buffer;
}

char *buffer_pushlstring(lua_State *L, char *start) {
    char *end = strchr(start, 0);
    lua_pushlstring(L, start, end - start);
    return end + 1;
}
