function make_absolute_url(options) {
    var loc = window.location;
    var protocol = get(options, "protocol", loc.protocol);
    if (protocol[protocol.length - 1] != ":") {
        protocol += ":";
    }

    var hostname = get(options, "hostname", loc.hostname);

    var subdomain = get(options, "subdomain");
    if (subdomain) {
        hostname = subdomain + "." + hostname;
    }

    var port = get(options, "port", loc.port)
    var path = get(options, "path", loc.pathname);
    var query = get(options, "query", loc.search);
    var hash = get(options, "hash", loc.hash)

    var url = protocol + "//" + hostname;
    if (port) {
        url += ":" + port;
    }

    if (path[0] != "/") {
        url += "/";
    }
    url += path;
    if (query) {
        if (query[0] != "?") {
            url += "?";
        }
        url += query;
    }
    if (hash) {
        if (hash[0] != "#") {
            url += "#";
        }
        url += hash;
    }
    return url;
}

function get(obj, name, default_val) {
    if (obj.hasOwnProperty(name)) {
        return obj[name];
    }
    return default_val;
}

function token() {
    var uuid = [to_hex(rand_int(32), 8),
                to_hex(rand_int(16), 4),
                to_hex(0x4000 | rand_int(12), 4),
                to_hex(0x8000 | rand_int(14), 4),
                to_hex(rand_int(48), 12)].join("-")
    return uuid;
}

function rand_int(bits) {
    if (bits < 1 || bits > 53) {
        throw new TypeError();
    } else {
        if (bits >= 1 && bits <= 30) {
            return 0 | ((1 << bits) * Math.random());
        } else {
            var high = (0 | ((1 << (bits - 30)) * Math.random())) * (1 << 30);
            var low = 0 | ((1 << 30) * Math.random());
            return  high + low;
        }
    }
}

function to_hex(x, length) {
    var rv = x.toString(16);
    while (rv.length < length) {
        rv = "0" + rv;
    }
    return rv;
}
