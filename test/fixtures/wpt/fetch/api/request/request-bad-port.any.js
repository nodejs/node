// META: global=window,worker

// list of bad ports according to
// https://fetch.spec.whatwg.org/#port-blocking
var BLOCKED_PORTS_LIST = [
    1,    // tcpmux
    7,    // echo
    9,    // discard
    11,   // systat
    13,   // daytime
    15,   // netstat
    17,   // qotd
    19,   // chargen
    20,   // ftp-data
    21,   // ftp
    22,   // ssh
    23,   // telnet
    25,   // smtp
    37,   // time
    42,   // name
    43,   // nicname
    53,   // domain
    77,   // priv-rjs
    79,   // finger
    87,   // ttylink
    95,   // supdup
    101,  // hostriame
    102,  // iso-tsap
    103,  // gppitnp
    104,  // acr-nema
    109,  // pop2
    110,  // pop3
    111,  // sunrpc
    113,  // auth
    115,  // sftp
    117,  // uucp-path
    119,  // nntp
    123,  // ntp
    135,  // loc-srv / epmap
    139,  // netbios
    143,  // imap2
    179,  // bgp
    389,  // ldap
    427,  // afp (alternate)
    465,  // smtp (alternate)
    512,  // print / exec
    513,  // login
    514,  // shell
    515,  // printer
    526,  // tempo
    530,  // courier
    531,  // chat
    532,  // netnews
    540,  // uucp
    548,  // afp
    556,  // remotefs
    563,  // nntp+ssl
    587,  // smtp (outgoing)
    601,  // syslog-conn
    636,  // ldap+ssl
    993,  // ldap+ssl
    995,  // pop3+ssl
    1720, // h323hostcall
    1723, // pptp
    2049, // nfs
    3659, // apple-sasl
    4045, // lockd
    5060, // sip
    5061, // sips
    6000, // x11
    6665, // irc (alternate)
    6666, // irc (alternate)
    6667, // irc (default)
    6668, // irc (alternate)
    6669, // irc (alternate)
    6697, // irc+tls
];

BLOCKED_PORTS_LIST.map(function(a){
    promise_test(function(t){
        return promise_rejects_js(t, TypeError, fetch("http://example.com:" + a))
    }, 'Request on bad port ' + a + ' should throw TypeError.');
});
