function FindProxyForURL(url, host) {
    return "PROXY {{host}}:{{ports[http][0]}}"
}
