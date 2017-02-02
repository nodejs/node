module.exports = [
  {
    'url': 'tftp://foobar.com/someconfig;mode=netascii',
    'protocol': 'tftp:',
    'hostname': 'foobar.com',
    'pathname': '/someconfig;mode=netascii'
  },
  {
    'url': 'telnet://user:pass@foobar.com:23/',
    'protocol': 'telnet:',
    'username': 'user',
    'password': 'pass',
    'hostname': 'foobar.com',
    'port': '23',
    'pathname': '/'
  },
  {
    'url': 'ut2004://10.10.10.10:7777/Index.ut2',
    'protocol': 'ut2004:',
    'hostname': '10.10.10.10',
    'port': '7777',
    'pathname': '/Index.ut2'
  },
  {
    'url': 'redis://foo:bar@somehost:6379/0?baz=bam&qux=baz',
    'protocol': 'redis:',
    'username': 'foo',
    'password': 'bar',
    'hostname': 'somehost',
    'port': '6379',
    'pathname': '/0',
    'search': '?baz=bam&qux=baz'
  },
  {
    'url': 'rsync://foo@host:911/sup',
    'protocol': 'rsync:',
    'username': 'foo',
    'hostname': 'host',
    'port': '911',
    'pathname': '/sup'
  },
  {
    'url': 'git://github.com/foo/bar.git',
    'protocol': 'git:',
    'hostname': 'github.com',
    'pathname': '/foo/bar.git'
  },
  {
    'url': 'irc://myserver.com:6999/channel?passwd',
    'protocol': 'irc:',
    'hostname': 'myserver.com',
    'port': '6999',
    'pathname': '/channel',
    'search': '?passwd'
  },
  {
    'url': 'dns://fw.example.org:9999/foo.bar.org?type=TXT',
    'protocol': 'dns:',
    'hostname': 'fw.example.org',
    'port': '9999',
    'pathname': '/foo.bar.org',
    'search': '?type=TXT'
  },
  {
    'url': 'ldap://localhost:389/ou=People,o=JNDITutorial',
    'protocol': 'ldap:',
    'hostname': 'localhost',
    'port': '389',
    'pathname': '/ou=People,o=JNDITutorial'
  },
  {
    'url': 'git+https://github.com/foo/bar',
    'protocol': 'git+https:',
    'hostname': 'github.com',
    'pathname': '/foo/bar'
  },
  {
    'url': 'urn:ietf:rfc:2648',
    'protocol': 'urn:',
    'pathname': 'ietf:rfc:2648'
  },
  {
    'url': 'tag:joe@example.org,2001:foo/bar',
    'protocol': 'tag:',
    'pathname': 'joe@example.org,2001:foo/bar'
  }
];
