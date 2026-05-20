import binascii
import logging
import sys
from collections.abc import Iterator
from typing import Dict, Any, Iterable


log = logging.getLogger(__name__)


class ParseError(Exception):
    pass

def _get_int(d, n):
    if len(d) < n:
        raise ParseError(f'get_int: {n} bytes needed, but data is {d}')
    if n == 1:
        dlen = d[0]
    else:
        dlen = int.from_bytes(d[0:n], byteorder='big')
    return d[n:], dlen


def _get_field(d, dlen):
    if dlen > 0:
        if len(d) < dlen:
            raise ParseError(f'field len={dlen}, but data len={len(d)}')
        field = d[0:dlen]
        return d[dlen:], field
    return d, b''


def _get_len_field(d, n):
    d, dlen = _get_int(d, n)
    return _get_field(d, dlen)


# d are bytes that start with a quic variable length integer
def _get_qint(d):
    i = d[0] & 0xc0
    if i == 0:
        return d[1:], int(d[0])
    elif i == 0x40:
        ndata = bytearray(d[0:2])
        d = d[2:]
        ndata[0] = ndata[0] & ~0xc0
        return d, int.from_bytes(ndata, byteorder='big')
    elif i == 0x80:
        ndata = bytearray(d[0:4])
        d = d[4:]
        ndata[0] = ndata[0] & ~0xc0
        return d, int.from_bytes(ndata, byteorder='big')
    else:
        ndata = bytearray(d[0:8])
        d = d[8:]
        ndata[0] = ndata[0] & ~0xc0
        return d, int.from_bytes(ndata, byteorder='big')


class TlsSupportedGroups:
    NAME_BY_ID = {
        0: 'Reserved',
        1: 'sect163k1',
        2: 'sect163r1',
        3: 'sect163r2',
        4: 'sect193r1',
        5: 'sect193r2',
        6: 'sect233k1',
        7: 'sect233r1',
        8: 'sect239k1',
        9: 'sect283k1',
        10: 'sect283r1',
        11: 'sect409k1',
        12: 'sect409r1',
        13: 'sect571k1',
        14: 'sect571r1',
        15: 'secp160k1',
        16: 'secp160r1',
        17: 'secp160r2',
        18: 'secp192k1',
        19: 'secp192r1',
        20: 'secp224k1',
        21: 'secp224r1',
        22: 'secp256k1',
        23: 'secp256r1',
        24: 'secp384r1',
        25: 'secp521r1',
        26: 'brainpoolP256r1',
        27: 'brainpoolP384r1',
        28: 'brainpoolP512r1',
        29: 'x25519',
        30: 'x448',
        31: 'brainpoolP256r1tls13',
        32: 'brainpoolP384r1tls13',
        33: 'brainpoolP512r1tls13',
        34: 'GC256A',
        35: 'GC256B',
        36: 'GC256C',
        37: 'GC256D',
        38: 'GC512A',
        39: 'GC512B',
        40: 'GC512C',
        41: 'curveSM2',
    }

    @classmethod
    def name(cls, gid):
        if gid in cls.NAME_BY_ID:
            return f'{cls.NAME_BY_ID[gid]}(0x{gid:0x})'
        return f'0x{gid:0x}'


class TlsSignatureScheme:
    NAME_BY_ID = {
        0x0201: 'rsa_pkcs1_sha1',
        0x0202: 'Reserved',
        0x0203:	'ecdsa_sha1',
        0x0401: 'rsa_pkcs1_sha256',
        0x0403: 'ecdsa_secp256r1_sha256',
        0x0420:	'rsa_pkcs1_sha256_legacy',
        0x0501: 'rsa_pkcs1_sha384',
        0x0503:	'ecdsa_secp384r1_sha384',
        0x0520: 'rsa_pkcs1_sha384_legacy',
        0x0601:	'rsa_pkcs1_sha512',
        0x0603: 'ecdsa_secp521r1_sha512',
        0x0620:	'rsa_pkcs1_sha512_legacy',
        0x0704: 'eccsi_sha256',
        0x0705:	'iso_ibs1',
        0x0706: 'iso_ibs2',
        0x0707:	'iso_chinese_ibs',
        0x0708: 'sm2sig_sm3',
        0x0709:	'gostr34102012_256a',
        0x070A: 'gostr34102012_256b',
        0x070B:	'gostr34102012_256c',
        0x070C: 'gostr34102012_256d',
        0x070D:	'gostr34102012_512a',
        0x070E: 'gostr34102012_512b',
        0x070F:	'gostr34102012_512c',
        0x0804: 'rsa_pss_rsae_sha256',
        0x0805:	'rsa_pss_rsae_sha384',
        0x0806: 'rsa_pss_rsae_sha512',
        0x0807:	'ed25519',
        0x0808: 'ed448',
        0x0809:	'rsa_pss_pss_sha256',
        0x080A: 'rsa_pss_pss_sha384',
        0x080B:	'rsa_pss_pss_sha512',
        0x081A: 'ecdsa_brainpoolP256r1tls13_sha256',
        0x081B:	'ecdsa_brainpoolP384r1tls13_sha384',
        0x081C: 'ecdsa_brainpoolP512r1tls13_sha512',
    }

    @classmethod
    def name(cls, gid):
        if gid in cls.NAME_BY_ID:
            return f'{cls.NAME_BY_ID[gid]}(0x{gid:0x})'
        return f'0x{gid:0x}'


class TlsCipherSuites:
    NAME_BY_ID = {
        0x1301: 'TLS_AES_128_GCM_SHA256',
        0x1302: 'TLS_AES_256_GCM_SHA384',
        0x1303: 'TLS_CHACHA20_POLY1305_SHA256',
        0x1304: 'TLS_AES_128_CCM_SHA256',
        0x1305: 'TLS_AES_128_CCM_8_SHA256',
        0x00ff: 'TLS_EMPTY_RENEGOTIATION_INFO_SCSV',
    }

    @classmethod
    def name(cls, cid):
        if cid in cls.NAME_BY_ID:
            return f'{cls.NAME_BY_ID[cid]}(0x{cid:0x})'
        return f'Cipher(0x{cid:0x})'


class PskKeyExchangeMode:
    NAME_BY_ID = {
        0x00: 'psk_ke',
        0x01: 'psk_dhe_ke',
    }

    @classmethod
    def name(cls, gid):
        if gid in cls.NAME_BY_ID:
            return f'{cls.NAME_BY_ID[gid]}(0x{gid:0x})'
        return f'0x{gid:0x}'


class QuicTransportParam:
    NAME_BY_ID = {
        0x00: 'original_destination_connection_id',
        0x01: 'max_idle_timeout',
        0x02: 'stateless_reset_token',
        0x03: 'max_udp_payload_size',
        0x04: 'initial_max_data',
        0x05: 'initial_max_stream_data_bidi_local',
        0x06: 'initial_max_stream_data_bidi_remote',
        0x07: 'initial_max_stream_data_uni',
        0x08: 'initial_max_streams_bidi',
        0x09: 'initial_max_streams_uni',
        0x0a: 'ack_delay_exponent',
        0x0b: 'max_ack_delay',
        0x0c: 'disable_active_migration',
        0x0d: 'preferred_address',
        0x0e: 'active_connection_id_limit',
        0x0f: 'initial_source_connection_id',
        0x10: 'retry_source_connection_id',
    }
    TYPE_BY_ID = {
        0x00: bytes,
        0x01: int,
        0x02: bytes,
        0x03: int,
        0x04: int,
        0x05: int,
        0x06: int,
        0x07: int,
        0x08: int,
        0x09: int,
        0x0a: int,
        0x0b: int,
        0x0c: int,
        0x0d: bytes,
        0x0e: int,
        0x0f: bytes,
        0x10: bytes,
    }

    @classmethod
    def name(cls, cid):
        if cid in cls.NAME_BY_ID:
            return f'{cls.NAME_BY_ID[cid]}(0x{cid:0x})'
        return f'QuicTP(0x{cid:0x})'

    @classmethod
    def is_qint(cls, cid):
        if cid in cls.TYPE_BY_ID:
            return cls.TYPE_BY_ID[cid] == int
        return False


class Extension:

    def __init__(self, eid, name, edata, hsid):
        self._eid = eid
        self._name = name
        self._edata = edata
        self._hsid = hsid

    @property
    def data(self):
        return self._edata

    @property
    def hsid(self):
        return self._hsid

    def to_json(self):
        jdata = {
            'id': self._eid,
            'name': self._name,
        }
        if len(self.data) > 0:
            jdata['data'] = binascii.hexlify(self.data).decode()
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        s = f'{ind}{self._name}(0x{self._eid:0x})'
        if len(self._edata):
            s += f'\n{ind}  data({len(self._edata)}): ' \
                   f'{binascii.hexlify(self._edata).decode()}'
        return s


class ExtSupportedGroups(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = edata
        self._groups = []
        while len(d) > 0:
            d, gid = _get_int(d, 2)
            self._groups.append(gid)

    def to_json(self):
        jdata = {
            'id': self._eid,
            'name': self._name,
        }
        if len(self._groups):
            jdata['groups'] = [TlsSupportedGroups.name(gid)
                               for gid in self._groups]
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        gnames = [TlsSupportedGroups.name(gid) for gid in self._groups]
        s = f'{ind}{self._name}(0x{self._eid:0x}): {", ".join(gnames)}'
        return s


class ExtKeyShare(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = self.data
        self._keys = []
        self._group = None
        self._pubkey = None
        if self.hsid == 2:  # ServerHello
            # single key share (group, pubkey)
            d, self._group = _get_int(d, 2)
            d, self._pubkey = _get_len_field(d, 2)
        elif self.hsid == 6:  # HelloRetryRequest
            assert len(d) == 2
            d, self._group = _get_int(d, 2)
        else:
            # list if key shares (group, pubkey)
            d, shares = _get_len_field(d, 2)
            while len(shares) > 0:
                shares, group = _get_int(shares, 2)
                shares, pubkey = _get_len_field(shares, 2)
                self._keys.append({
                    'group': TlsSupportedGroups.name(group),
                    'pubkey': binascii.hexlify(pubkey).decode()
                })

    def to_json(self):
        jdata = super().to_json()
        if self._group is not None:
            jdata['group'] = TlsSupportedGroups.name(self._group)
        if self._pubkey is not None:
            jdata['pubkey'] = binascii.hexlify(self._pubkey).decode()
        if len(self._keys) > 0:
            jdata['keys'] = self._keys
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        s = f'{ind}{self._name}(0x{self._eid:0x})'
        if self._group is not None:
            s += f'\n{ind}  group: {TlsSupportedGroups.name(self._group)}'
        if self._pubkey is not None:
            s += f'\n{ind}  pubkey: {binascii.hexlify(self._pubkey).decode()}'
        if len(self._keys) > 0:
            for idx, key in enumerate(self._keys):
                s += f'\n{ind}    {idx}: {key["group"]}, {key["pubkey"]}'
        return s


class ExtSNI(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = self.data
        self._indicators = []
        while len(d) > 0:
            d, entry = _get_len_field(d, 2)
            entry, stype = _get_int(entry, 1)
            entry, sname = _get_len_field(entry, 2)
            self._indicators.append({
                'type': stype,
                'name': sname.decode()
            })

    def to_json(self):
        jdata = super().to_json()
        for i in self._indicators:
            if i['type'] == 0:
                jdata['host_name'] = i['name']
            else:
                jdata[f'0x{i["type"]}'] = i['name']
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        s = f'{ind}{self._name}(0x{self._eid:0x})'
        if len(self._indicators) == 1 and self._indicators[0]['type'] == 0:
            s += f': {self._indicators[0]["name"]}'
        else:
            for i in self._indicators:
                ikey = 'host_name' if i["type"] == 0 else f'type(0x{i["type"]:0x}'
                s += f'\n{ind}    {ikey}: {i["name"]}'
        return s


class ExtALPN(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = self.data
        d, list_len = _get_int(d, 2)
        self._protocols = []
        while len(d) > 0:
            d, proto = _get_len_field(d, 1)
            self._protocols.append(proto.decode())

    def to_json(self):
        jdata = super().to_json()
        if len(self._protocols) == 1:
            jdata['alpn'] = self._protocols[0]
        else:
            jdata['alpn'] = self._protocols
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return f'{ind}{self._name}(0x{self._eid:0x}): {", ".join(self._protocols)}'


class ExtEarlyData(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        self._max_size = None
        d = self.data
        if hsid == 4:  # SessionTicket
            assert len(d) == 4, f'expected 4, len is {len(d)} data={d}'
            d, self._max_size = _get_int(d, 4)
        else:
            assert len(d) == 0

    def to_json(self):
        jdata = super().to_json()
        if self._max_size is not None:
            jdata['max_size'] = self._max_size
        return jdata


class ExtSignatureAlgorithms(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = self.data
        d, list_len = _get_int(d, 2)
        self._algos = []
        while len(d) > 0:
            d, algo = _get_int(d, 2)
            self._algos.append(TlsSignatureScheme.name(algo))

    def to_json(self):
        jdata = super().to_json()
        if len(self._algos) > 0:
            jdata['algorithms'] = self._algos
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return f'{ind}{self._name}(0x{self._eid:0x}): {", ".join(self._algos)}'


class ExtPSKExchangeModes(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = self.data
        d, list_len = _get_int(d, 1)
        self._modes = []
        while len(d) > 0:
            d, mode = _get_int(d, 1)
            self._modes.append(PskKeyExchangeMode.name(mode))

    def to_json(self):
        jdata = super().to_json()
        jdata['modes'] = self._modes
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return f'{ind}{self._name}(0x{self._eid:0x}): {", ".join(self._modes)}'


class ExtPreSharedKey(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        self._kid = None
        self._identities = None
        self._binders = None
        d = self.data
        if hsid == 1:  # client hello
            d, idata = _get_len_field(d, 2)
            self._identities = []
            while len(idata):
                idata, identity = _get_len_field(idata, 2)
                idata, obfs_age = _get_int(idata, 4)
                self._identities.append({
                    'id': binascii.hexlify(identity).decode(),
                    'age': obfs_age,
                })
            d, binders = _get_len_field(d, 2)
            self._binders = []
            while len(binders) > 0:
                binders, hmac = _get_len_field(binders, 1)
                self._binders.append(binascii.hexlify(hmac).decode())
            assert len(d) == 0
        else:
            d, self._kid = _get_int(d, 2)

    def to_json(self):
        jdata = super().to_json()
        if self.hsid == 1:
            jdata['identities'] = self._identities
            jdata['binders'] = self._binders
        else:
            jdata['identity'] = self._kid
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        s = f'{ind}{self._name}(0x{self._hsid:0x})'
        if self.hsid == 1:
            for idx, i in enumerate(self._identities):
                s += f'\n{ind}  {idx}: {i["id"]} ({i["age"]})'
            s += f'\n{ind}  binders: {self._binders}'
        else:
            s += f'\n{ind}  identity: {self._kid}'
        return s


class ExtSupportedVersions(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = self.data
        self._versions = []
        if hsid == 1:  # client hello
            d, list_len = _get_int(d, 1)
            while len(d) > 0:
                d, version = _get_int(d, 2)
                self._versions.append(f'0x{version:0x}')
        else:
            d, version = _get_int(d, 2)
            self._versions.append(f'0x{version:0x}')

    def to_json(self):
        jdata = super().to_json()
        if len(self._versions) == 1:
            jdata['version'] = self._versions[0]
        else:
            jdata['versions'] = self._versions
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return f'{ind}{self._name}(0x{self._eid:0x}): {", ".join(self._versions)}'


class ExtQuicTP(Extension):

    def __init__(self, eid, name, edata, hsid):
        super().__init__(eid=eid, name=name, edata=edata, hsid=hsid)
        d = self.data
        self._params = []
        while len(d) > 0:
            d, ptype = _get_qint(d)
            d, plen = _get_qint(d)
            d, pvalue = _get_field(d, plen)
            if QuicTransportParam.is_qint(ptype):
                _, pvalue = _get_qint(pvalue)
            else:
                pvalue = binascii.hexlify(pvalue).decode()
            self._params.append({
                'key': QuicTransportParam.name(ptype),
                'value': pvalue,
            })

    def to_json(self):
        jdata = super().to_json()
        jdata['params'] = self._params
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        s = f'{ind}{self._name}(0x{self._eid:0x})'
        for p in self._params:
            s += f'\n{ind}  {p["key"]}: {p["value"]}'
        return s


class TlsExtensions:

    EXT_TYPES = [
        (0x00, 'SNI', ExtSNI),
        (0x01, 'MAX_FRAGMENT_LENGTH', Extension),
        (0x03, 'TRUSTED_CA_KEYS', Extension),
        (0x04, 'TRUNCATED_HMAC', Extension),
        (0x05, 'OSCP_STATUS_REQUEST', Extension),
        (0x0a, 'SUPPORTED_GROUPS', ExtSupportedGroups),
        (0x0b, 'EC_POINT_FORMATS', Extension),
        (0x0d, 'SIGNATURE_ALGORITHMS', ExtSignatureAlgorithms),
        (0x0e, 'USE_SRTP', Extension),
        (0x10, 'ALPN', ExtALPN),
        (0x11, 'STATUS_REQUEST_V2', Extension),
        (0x16, 'ENCRYPT_THEN_MAC', Extension),
        (0x17, 'EXTENDED_MASTER_SECRET', Extension),
        (0x23, 'SESSION_TICKET', Extension),
        (0x29, 'PRE_SHARED_KEY', ExtPreSharedKey),
        (0x2a, 'EARLY_DATA', ExtEarlyData),
        (0x2b, 'SUPPORTED_VERSIONS', ExtSupportedVersions),
        (0x2c, 'COOKIE', Extension),
        (0x2d, 'PSK_KEY_EXCHANGE_MODES', ExtPSKExchangeModes),
        (0x31, 'POST_HANDSHAKE_AUTH', Extension),
        (0x32, 'SIGNATURE_ALGORITHMS_CERT', Extension),
        (0x33, 'KEY_SHARE', ExtKeyShare),
        (0x39, 'QUIC_TP_PARAMS', ExtQuicTP),
        (0xff01, 'RENEGOTIATION_INFO', Extension),
    ]
    NAME_BY_ID = {}
    CLASS_BY_ID = {}

    @classmethod
    def init(cls):
        for (eid, name, ecls) in cls.EXT_TYPES:
            cls.NAME_BY_ID[eid] = name
            cls.CLASS_BY_ID[eid] = ecls

    @classmethod
    def from_data(cls, hsid, data):
        exts = []
        d = data
        while len(d):
            d, eid = _get_int(d, 2)
            d, elen = _get_int(d, 2)
            d, edata = _get_field(d, elen)
            if eid in cls.NAME_BY_ID:
                ename = cls.NAME_BY_ID[eid]
                ecls = cls.CLASS_BY_ID[eid]
                exts.append(ecls(eid=eid, name=ename, edata=edata, hsid=hsid))
            else:
                exts.append(Extension(eid=eid, name=f'(0x{eid:0x})',
                                      edata=edata, hsid=hsid))
        return exts


TlsExtensions.init()


class HSRecord:

    def __init__(self, hsid: int, name: str, data):
        self._hsid = hsid
        self._name = name
        self._data = data

    @property
    def hsid(self):
        return self._hsid

    @property
    def name(self):
        return self._name

    @name.setter
    def name(self, value):
        self._name = value

    @property
    def data(self):
        return self._data

    def __repr__(self):
        return f'{self.name}[{binascii.hexlify(self._data).decode()}]'

    def to_json(self) -> Dict[str, Any]:
        return {
            'name': self.name,
            'data': binascii.hexlify(self._data).decode(),
        }

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return f'{ind}{self._name}\n'\
               f'{ind}  id: 0x{self._hsid:0x}\n'\
               f'{ind}  data({len(self._data)}): '\
               f'{binascii.hexlify(self._data).decode()}'


class ClientHello(HSRecord):

    def __init__(self, hsid: int, name: str, data):
        super().__init__(hsid=hsid, name=name, data=data)
        d = data
        d, self._version = _get_int(d, 2)
        d, self._random = _get_field(d, 32)
        d, self._session_id = _get_len_field(d, 1)
        self._ciphers = []
        d, ciphers = _get_len_field(d, 2)
        while len(ciphers):
            ciphers, cipher = _get_int(ciphers, 2)
            self._ciphers.append(TlsCipherSuites.name(cipher))
        d, comps = _get_len_field(d, 1)
        self._compressions = [int(c) for c in comps]
        d, edata = _get_len_field(d, 2)
        self._extensions = TlsExtensions.from_data(hsid, edata)

    def to_json(self):
        jdata = super().to_json()
        jdata['version'] = f'0x{self._version:0x}'
        jdata['random'] = f'{binascii.hexlify(self._random).decode()}'
        jdata['session_id'] = binascii.hexlify(self._session_id).decode()
        jdata['ciphers'] = self._ciphers
        jdata['compressions'] = self._compressions
        jdata['extensions'] = [ext.to_json() for ext in self._extensions]
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return super().to_text(indent=indent) + '\n'\
            f'{ind}  version: 0x{self._version:0x}\n'\
            f'{ind}  random: {binascii.hexlify(self._random).decode()}\n' \
            f'{ind}  session_id: {binascii.hexlify(self._session_id).decode()}\n' \
            f'{ind}  ciphers: {", ".join(self._ciphers)}\n'\
            f'{ind}  compressions: {self._compressions}\n'\
            f'{ind}  extensions: \n' + '\n'.join(
                [ext.to_text(indent=indent+4) for ext in self._extensions])


class ServerHello(HSRecord):

    HELLO_RETRY_RANDOM = binascii.unhexlify(
        'CF21AD74E59A6111BE1D8C021E65B891C2A211167ABB8C5E079E09E2C8A8339C'
    )

    def __init__(self, hsid: int, name: str, data):
        super().__init__(hsid=hsid, name=name, data=data)
        d = data
        d, self._version = _get_int(d, 2)
        d, self._random = _get_field(d, 32)
        if self._random == self.HELLO_RETRY_RANDOM:
            self.name = 'HelloRetryRequest'
            hsid = 6
        d, self._session_id = _get_len_field(d, 1)
        d, cipher = _get_int(d, 2)
        self._cipher = TlsCipherSuites.name(cipher)
        d, self._compression = _get_int(d, 1)
        d, edata = _get_len_field(d, 2)
        self._extensions = TlsExtensions.from_data(hsid, edata)

    def to_json(self):
        jdata = super().to_json()
        jdata['version'] = f'0x{self._version:0x}'
        jdata['random'] = f'{binascii.hexlify(self._random).decode()}'
        jdata['session_id'] = binascii.hexlify(self._session_id).decode()
        jdata['cipher'] = self._cipher
        jdata['compression'] = int(self._compression)
        jdata['extensions'] = [ext.to_json() for ext in self._extensions]
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return super().to_text(indent=indent) + '\n'\
            f'{ind}  version: 0x{self._version:0x}\n'\
            f'{ind}  random: {binascii.hexlify(self._random).decode()}\n' \
            f'{ind}  session_id: {binascii.hexlify(self._session_id).decode()}\n' \
            f'{ind}  cipher: {self._cipher}\n'\
            f'{ind}  compression: {int(self._compression)}\n'\
            f'{ind}  extensions: \n' + '\n'.join(
                [ext.to_text(indent=indent+4) for ext in self._extensions])


class EncryptedExtensions(HSRecord):

    def __init__(self, hsid: int, name: str, data):
        super().__init__(hsid=hsid, name=name, data=data)
        d = data
        d, edata = _get_len_field(d, 2)
        self._extensions = TlsExtensions.from_data(hsid, edata)

    def to_json(self):
        jdata = super().to_json()
        jdata['extensions'] = [ext.to_json() for ext in self._extensions]
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return super().to_text(indent=indent) + '\n'\
            f'{ind}  extensions: \n' + '\n'.join(
                [ext.to_text(indent=indent+4) for ext in self._extensions])


class CertificateRequest(HSRecord):

    def __init__(self, hsid: int, name: str, data):
        super().__init__(hsid=hsid, name=name, data=data)
        d = data
        d, self._context = _get_int(d, 1)
        d, edata = _get_len_field(d, 2)
        self._extensions = TlsExtensions.from_data(hsid, edata)

    def to_json(self):
        jdata = super().to_json()
        jdata['context'] = self._context
        jdata['extensions'] = [ext.to_json() for ext in self._extensions]
        return jdata

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return super().to_text(indent=indent) + '\n'\
            f'{ind}  context: {self._context}\n'\
            f'{ind}  extensions: \n' + '\n'.join(
                [ext.to_text(indent=indent+4) for ext in self._extensions])


class Certificate(HSRecord):

    def __init__(self, hsid: int, name: str, data):
        super().__init__(hsid=hsid, name=name, data=data)
        d = data
        d, self._context = _get_int(d, 1)
        d, clist = _get_len_field(d, 3)
        self._cert_entries = []
        while len(clist) > 0:
            clist, cert_data = _get_len_field(clist, 3)
            clist, cert_exts = _get_len_field(clist, 2)
            exts = TlsExtensions.from_data(hsid, cert_exts)
            self._cert_entries.append({
                'cert': binascii.hexlify(cert_data).decode(),
                'extensions': exts,
            })

    def to_json(self):
        jdata = super().to_json()
        jdata['context'] = self._context
        jdata['certificate_list'] = [{
            'cert': e['cert'],
            'extensions': [x.to_json() for x in e['extensions']],
        } for e in self._cert_entries]
        return jdata

    def _enxtry_text(self, e, indent: int = 0):
        ind = ' ' * (indent + 2)
        return f'{ind} cert: {e["cert"]}\n'\
               f'{ind}  extensions: \n' + '\n'.join(
            [x.to_text(indent=indent + 4) for x in e['extensions']])

    def to_text(self, indent: int = 0):
        ind = ' ' * (indent + 2)
        return super().to_text(indent=indent) + '\n'\
            f'{ind}  context: {self._context}\n'\
            f'{ind}  certificate_list: \n' + '\n'.join(
                [self._enxtry_text(e, indent+4) for e in self._cert_entries])


class SessionTicket(HSRecord):

    def __init__(self, hsid: int, name: str, data):
        super().__init__(hsid=hsid, name=name, data=data)
        d = data
        d, self._lifetime = _get_int(d, 4)
        d, self._age = _get_int(d, 4)
        d, self._nonce = _get_len_field(d, 1)
        d, self._ticket = _get_len_field(d, 2)
        d, edata = _get_len_field(d, 2)
        self._extensions = TlsExtensions.from_data(hsid, edata)

    def to_json(self):
        jdata = super().to_json()
        jdata['lifetime'] = self._lifetime
        jdata['age'] = self._age
        jdata['nonce'] = binascii.hexlify(self._nonce).decode()
        jdata['ticket'] = binascii.hexlify(self._ticket).decode()
        jdata['extensions'] = [ext.to_json() for ext in self._extensions]
        return jdata


class HSIterator(Iterator):

    def __init__(self, recs):
        self._recs = recs
        self._index = 0

    def __iter__(self):
        return self

    def __next__(self):
        try:
            result = self._recs[self._index]
        except IndexError:
            raise StopIteration
        self._index += 1
        return result


class HandShake:
    REC_TYPES = [
        (1, 'ClientHello', ClientHello),
        (2, 'ServerHello', ServerHello),
        (3, 'HelloVerifyRequest', HSRecord),
        (4, 'SessionTicket', SessionTicket),
        (5, 'EndOfEarlyData', HSRecord),
        (6, 'HelloRetryRequest', ServerHello),
        (8, 'EncryptedExtensions', EncryptedExtensions),
        (11, 'Certificate', Certificate),
        (12, 'ServerKeyExchange ', HSRecord),
        (13, 'CertificateRequest', CertificateRequest),
        (14, 'ServerHelloDone', HSRecord),
        (15, 'CertificateVerify', HSRecord),
        (16, 'ClientKeyExchange', HSRecord),
        (20, 'Finished', HSRecord),
        (22, 'CertificateStatus', HSRecord),
        (24, 'KeyUpdate', HSRecord),
        (25, 'CompressedCertificate', HSRecord),
    ]
    RT_NAME_BY_ID = {}
    RT_CLS_BY_ID = {}

    @classmethod
    def _parse_rec(cls, data):
        d, hsid = _get_int(data, 1)
        if hsid not in cls.RT_CLS_BY_ID:
            raise ParseError(f'unknown type {hsid}')
        d, rec_len = _get_int(d, 3)
        if rec_len > len(d):
            # incomplete, need more data
            return data, None
        d, rec_data = _get_field(d, rec_len)
        if hsid in cls.RT_CLS_BY_ID:
            name = cls.RT_NAME_BY_ID[hsid]
            rcls = cls.RT_CLS_BY_ID[hsid]
        else:
            name = f'CryptoRecord(0x{hsid:0x})'
            rcls = HSRecord
        return d, rcls(hsid=hsid, name=name, data=rec_data)

    @classmethod
    def _parse(cls, source, strict=False, verbose: int = 0):
        d = b''
        hsid = 0
        hsrecs = []
        if verbose > 0:
            log.debug(f'scanning for handshake records')
        blocks = [d for d in source]
        while len(blocks) > 0:
            try:
                total_data = b''.join(blocks)
                remain, r = cls._parse_rec(total_data)
                if r is None:
                    # if we could not recognize a record, skip the first
                    # data block and try again
                    blocks = blocks[1:]
                    continue
                hsrecs.append(r)
                cons_len = len(total_data) - len(remain)
                while cons_len > 0 and len(blocks) > 0:
                    if cons_len >= len(blocks[0]):
                        cons_len -= len(blocks[0])
                        blocks = blocks[1:]
                    else:
                        blocks[0] = blocks[0][cons_len:]
                        cons_len = 0
                if verbose > 2:
                    log.debug(f'added record: {r.to_text()}')
            except ParseError as err:
                # if we could not recognize a record, skip the first
                # data block and try again
                blocks = blocks[1:]
        if len(blocks) > 0 and strict:
            raise Exception(f'possibly incomplete handshake record '
                            f'id={hsid}, from raw={blocks}\n')
        return hsrecs



    @classmethod
    def init(cls):
        for (hsid, name, rcls) in cls.REC_TYPES:
            cls.RT_NAME_BY_ID[hsid] = name
            cls.RT_CLS_BY_ID[hsid] = rcls

    def __init__(self, source: Iterable[bytes], strict: bool = False,
                 verbose: int = 0):
        self._source = source
        self._strict = strict
        self._verbose = verbose

    def __iter__(self):
        return HSIterator(recs=self._parse(self._source, strict=self._strict,
                                           verbose=self._verbose))


HandShake.init()
