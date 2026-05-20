import os
import re
from datetime import timedelta, datetime
from typing import List, Any, Optional

from cryptography import x509
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.hazmat.primitives.asymmetric.ec import EllipticCurvePrivateKey
from cryptography.hazmat.primitives.asymmetric.rsa import RSAPrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, PrivateFormat, NoEncryption, load_pem_private_key
from cryptography.x509 import ExtendedKeyUsageOID, NameOID


EC_SUPPORTED = {}
EC_SUPPORTED.update([(curve.name.upper(), curve) for curve in [
    ec.SECP192R1,
    ec.SECP224R1,
    ec.SECP256R1,
    ec.SECP384R1,
]])


def _private_key(key_type):
    if isinstance(key_type, str):
        key_type = key_type.upper()
        m = re.match(r'^(RSA)?(\d+)$', key_type)
        if m:
            key_type = int(m.group(2))

    if isinstance(key_type, int):
        return rsa.generate_private_key(
            public_exponent=65537,
            key_size=key_type,
            backend=default_backend()
        )
    if not isinstance(key_type, ec.EllipticCurve) and key_type in EC_SUPPORTED:
        key_type = EC_SUPPORTED[key_type]
    return ec.generate_private_key(
        curve=key_type,
        backend=default_backend()
    )


class CertificateSpec:

    def __init__(self, name: str = None, domains: List[str] = None,
                 email: str = None,
                 key_type: str = None, single_file: bool = False,
                 valid_from: timedelta = timedelta(days=-1),
                 valid_to: timedelta = timedelta(days=89),
                 client: bool = False,
                 sub_specs: List['CertificateSpec'] = None):
        self._name = name
        self.domains = domains
        self.client = client
        self.email = email
        self.key_type = key_type
        self.single_file = single_file
        self.valid_from = valid_from
        self.valid_to = valid_to
        self.sub_specs = sub_specs

    @property
    def name(self) -> Optional[str]:
        if self._name:
            return self._name
        elif self.domains:
            return self.domains[0]
        return None

    @property
    def type(self) -> Optional[str]:
        if self.domains and len(self.domains):
            return "server"
        elif self.client:
            return "client"
        elif self.name:
            return "ca"
        return None


class Credentials:

    def __init__(self, name: str, cert: Any, pkey: Any, issuer: 'Credentials' = None):
        self._name = name
        self._cert = cert
        self._pkey = pkey
        self._issuer = issuer
        self._cert_file = None
        self._pkey_file = None
        self._store = None

    @property
    def name(self) -> str:
        return self._name

    @property
    def subject(self) -> x509.Name:
        return self._cert.subject

    @property
    def key_type(self):
        if isinstance(self._pkey, RSAPrivateKey):
            return f"rsa{self._pkey.key_size}"
        elif isinstance(self._pkey, EllipticCurvePrivateKey):
            return f"{self._pkey.curve.name}"
        else:
            raise Exception(f"unknown key type: {self._pkey}")

    @property
    def private_key(self) -> Any:
        return self._pkey

    @property
    def certificate(self) -> Any:
        return self._cert

    @property
    def cert_pem(self) -> bytes:
        return self._cert.public_bytes(Encoding.PEM)

    @property
    def pkey_pem(self) -> bytes:
        return self._pkey.private_bytes(
            Encoding.PEM,
            PrivateFormat.TraditionalOpenSSL if self.key_type.startswith('rsa') else PrivateFormat.PKCS8,
            NoEncryption())

    @property
    def issuer(self) -> Optional['Credentials']:
        return self._issuer

    def set_store(self, store: 'CertStore'):
        self._store = store

    def set_files(self, cert_file: str, pkey_file: str = None):
        self._cert_file = cert_file
        self._pkey_file = pkey_file

    @property
    def cert_file(self) -> str:
        return self._cert_file

    @property
    def pkey_file(self) -> Optional[str]:
        return self._pkey_file

    def get_first(self, name) -> Optional['Credentials']:
        creds = self._store.get_credentials_for_name(name) if self._store else []
        return creds[0] if len(creds) else None

    def get_credentials_for_name(self, name) -> List['Credentials']:
        return self._store.get_credentials_for_name(name) if self._store else []

    def issue_certs(self, specs: List[CertificateSpec],
                    chain: List['Credentials'] = None) -> List['Credentials']:
        return [self.issue_cert(spec=spec, chain=chain) for spec in specs]

    def issue_cert(self, spec: CertificateSpec, chain: List['Credentials'] = None) -> 'Credentials':
        key_type = spec.key_type if spec.key_type else self.key_type
        creds = None
        if self._store:
            creds = self._store.load_credentials(
                name=spec.name, key_type=key_type, single_file=spec.single_file, issuer=self)
        if creds is None:
            creds = Ngtcp2TestCA.create_credentials(spec=spec, issuer=self, key_type=key_type,
                                                    valid_from=spec.valid_from, valid_to=spec.valid_to)
            if self._store:
                self._store.save(creds, single_file=spec.single_file)
                if spec.type == "ca":
                    self._store.save_chain(creds, "ca", with_root=True)

        if spec.sub_specs:
            if self._store:
                sub_store = CertStore(fpath=os.path.join(self._store.path, creds.name))
                creds.set_store(sub_store)
            subchain = chain.copy() if chain else []
            subchain.append(self)
            creds.issue_certs(spec.sub_specs, chain=subchain)
        return creds


class CertStore:

    def __init__(self, fpath: str):
        self._store_dir = fpath
        if not os.path.exists(self._store_dir):
            os.makedirs(self._store_dir)
        self._creds_by_name = {}

    @property
    def path(self) -> str:
        return self._store_dir

    def save(self, creds: Credentials, name: str = None,
             chain: List[Credentials] = None,
             single_file: bool = False) -> None:
        name = name if name is not None else creds.name
        cert_file = self.get_cert_file(name=name, key_type=creds.key_type)
        pkey_file = self.get_pkey_file(name=name, key_type=creds.key_type)
        if single_file:
            pkey_file = None
        with open(cert_file, "wb") as fd:
            fd.write(creds.cert_pem)
            if chain:
                for c in chain:
                    fd.write(c.cert_pem)
            if pkey_file is None:
                fd.write(creds.pkey_pem)
        if pkey_file is not None:
            with open(pkey_file, "wb") as fd:
                fd.write(creds.pkey_pem)
        creds.set_files(cert_file, pkey_file)
        self._add_credentials(name, creds)

    def save_chain(self, creds: Credentials, infix: str, with_root=False):
        name = creds.name
        chain = [creds]
        while creds.issuer is not None:
            creds = creds.issuer
            chain.append(creds)
        if not with_root and len(chain) > 1:
            chain = chain[:-1]
        chain_file = os.path.join(self._store_dir, f'{name}-{infix}.pem')
        with open(chain_file, "wb") as fd:
            for c in chain:
                fd.write(c.cert_pem)

    def _add_credentials(self, name: str, creds: Credentials):
        if name not in self._creds_by_name:
            self._creds_by_name[name] = []
        self._creds_by_name[name].append(creds)

    def get_credentials_for_name(self, name) -> List[Credentials]:
        return self._creds_by_name[name] if name in self._creds_by_name else []

    def get_cert_file(self, name: str, key_type=None) -> str:
        key_infix = ".{0}".format(key_type) if key_type is not None else ""
        return os.path.join(self._store_dir, f'{name}{key_infix}.cert.pem')

    def get_pkey_file(self, name: str, key_type=None) -> str:
        key_infix = ".{0}".format(key_type) if key_type is not None else ""
        return os.path.join(self._store_dir, f'{name}{key_infix}.pkey.pem')

    def load_pem_cert(self, fpath: str) -> x509.Certificate:
        with open(fpath) as fd:
            return x509.load_pem_x509_certificate("".join(fd.readlines()).encode())

    def load_pem_pkey(self, fpath: str):
        with open(fpath) as fd:
            return load_pem_private_key("".join(fd.readlines()).encode(), password=None)

    def load_credentials(self, name: str, key_type=None, single_file: bool = False, issuer: Credentials = None):
        cert_file = self.get_cert_file(name=name, key_type=key_type)
        pkey_file = cert_file if single_file else self.get_pkey_file(name=name, key_type=key_type)
        if os.path.isfile(cert_file) and os.path.isfile(pkey_file):
            cert = self.load_pem_cert(cert_file)
            pkey = self.load_pem_pkey(pkey_file)
            creds = Credentials(name=name, cert=cert, pkey=pkey, issuer=issuer)
            creds.set_store(self)
            creds.set_files(cert_file, pkey_file)
            self._add_credentials(name, creds)
            return creds
        return None


class Ngtcp2TestCA:

    @classmethod
    def create_root(cls, name: str, store_dir: str, key_type: str = "rsa2048") -> Credentials:
        store = CertStore(fpath=store_dir)
        creds = store.load_credentials(name="ca", key_type=key_type, issuer=None)
        if creds is None:
            creds = Ngtcp2TestCA._make_ca_credentials(name=name, key_type=key_type)
            store.save(creds, name="ca")
            creds.set_store(store)
        return creds

    @staticmethod
    def create_credentials(spec: CertificateSpec, issuer: Credentials, key_type: Any,
                           valid_from: timedelta = timedelta(days=-1),
                           valid_to: timedelta = timedelta(days=89),
                           ) -> Credentials:
        """Create a certificate signed by this CA for the given domains.
        :returns: the certificate and private key PEM file paths
        """
        if spec.domains and len(spec.domains):
            creds = Ngtcp2TestCA._make_server_credentials(name=spec.name, domains=spec.domains,
                                                          issuer=issuer, valid_from=valid_from,
                                                          valid_to=valid_to, key_type=key_type)
        elif spec.client:
            creds = Ngtcp2TestCA._make_client_credentials(name=spec.name, issuer=issuer,
                                                          email=spec.email, valid_from=valid_from,
                                                          valid_to=valid_to, key_type=key_type)
        elif spec.name:
            creds = Ngtcp2TestCA._make_ca_credentials(name=spec.name, issuer=issuer,
                                                      valid_from=valid_from, valid_to=valid_to,
                                                      key_type=key_type)
        else:
            raise Exception(f"unrecognized certificate specification: {spec}")
        return creds

    @staticmethod
    def _make_x509_name(org_name: str = None, common_name: str = None, parent: x509.Name = None) -> x509.Name:
        name_pieces = []
        if org_name:
            oid = NameOID.ORGANIZATIONAL_UNIT_NAME if parent else NameOID.ORGANIZATION_NAME
            name_pieces.append(x509.NameAttribute(oid, org_name))
        elif common_name:
            name_pieces.append(x509.NameAttribute(NameOID.COMMON_NAME, common_name))
        if parent:
            name_pieces.extend([rdn for rdn in parent])
        return x509.Name(name_pieces)

    @staticmethod
    def _make_csr(
            subject: x509.Name,
            pkey: Any,
            issuer_subject: Optional[Credentials],
            valid_from_delta: timedelta = None,
            valid_until_delta: timedelta = None
    ):
        pubkey = pkey.public_key()
        issuer_subject = issuer_subject if issuer_subject is not None else subject

        valid_from = datetime.now()
        if valid_until_delta is not None:
            valid_from += valid_from_delta
        valid_until = datetime.now()
        if valid_until_delta is not None:
            valid_until += valid_until_delta

        return (
            x509.CertificateBuilder()
            .subject_name(subject)
            .issuer_name(issuer_subject)
            .public_key(pubkey)
            .not_valid_before(valid_from)
            .not_valid_after(valid_until)
            .serial_number(x509.random_serial_number())
            .add_extension(
                x509.SubjectKeyIdentifier.from_public_key(pubkey),
                critical=False,
            )
        )

    @staticmethod
    def _add_ca_usages(csr: Any) -> Any:
        return csr.add_extension(
            x509.BasicConstraints(ca=True, path_length=9),
            critical=True,
        ).add_extension(
            x509.KeyUsage(
                digital_signature=True,
                content_commitment=False,
                key_encipherment=False,
                data_encipherment=False,
                key_agreement=False,
                key_cert_sign=True,
                crl_sign=True,
                encipher_only=False,
                decipher_only=False),
            critical=True
        ).add_extension(
            x509.ExtendedKeyUsage([
                ExtendedKeyUsageOID.CLIENT_AUTH,
                ExtendedKeyUsageOID.SERVER_AUTH,
                ExtendedKeyUsageOID.CODE_SIGNING,
            ]),
            critical=True
        )

    @staticmethod
    def _add_leaf_usages(csr: Any, domains: List[str], issuer: Credentials) -> Any:
        return csr.add_extension(
            x509.BasicConstraints(ca=False, path_length=None),
            critical=True,
        ).add_extension(
            x509.AuthorityKeyIdentifier.from_issuer_subject_key_identifier(
                issuer.certificate.extensions.get_extension_for_class(
                    x509.SubjectKeyIdentifier).value),
            critical=False
        ).add_extension(
            x509.SubjectAlternativeName([x509.DNSName(domain) for domain in domains]),
            critical=True,
        ).add_extension(
            x509.ExtendedKeyUsage([
                ExtendedKeyUsageOID.SERVER_AUTH,
            ]),
            critical=True
        )

    @staticmethod
    def _add_client_usages(csr: Any, issuer: Credentials, rfc82name: str = None) -> Any:
        cert = csr.add_extension(
            x509.BasicConstraints(ca=False, path_length=None),
            critical=True,
        ).add_extension(
            x509.AuthorityKeyIdentifier.from_issuer_subject_key_identifier(
                issuer.certificate.extensions.get_extension_for_class(
                    x509.SubjectKeyIdentifier).value),
            critical=False
        )
        if rfc82name:
            cert.add_extension(
                x509.SubjectAlternativeName([x509.RFC822Name(rfc82name)]),
                critical=True,
            )
        cert.add_extension(
            x509.ExtendedKeyUsage([
                ExtendedKeyUsageOID.CLIENT_AUTH,
            ]),
            critical=True
        )
        return cert

    @staticmethod
    def _make_ca_credentials(name, key_type: Any,
                             issuer: Credentials = None,
                             valid_from: timedelta = timedelta(days=-1),
                             valid_to: timedelta = timedelta(days=89),
                             ) -> Credentials:
        pkey = _private_key(key_type=key_type)
        if issuer is not None:
            issuer_subject = issuer.certificate.subject
            issuer_key = issuer.private_key
        else:
            issuer_subject = None
            issuer_key = pkey
        subject = Ngtcp2TestCA._make_x509_name(org_name=name, parent=issuer.subject if issuer else None)
        csr = Ngtcp2TestCA._make_csr(subject=subject,
                                     issuer_subject=issuer_subject, pkey=pkey,
                                     valid_from_delta=valid_from, valid_until_delta=valid_to)
        csr = Ngtcp2TestCA._add_ca_usages(csr)
        cert = csr.sign(private_key=issuer_key,
                        algorithm=hashes.SHA256(),
                        backend=default_backend())
        return Credentials(name=name, cert=cert, pkey=pkey, issuer=issuer)

    @staticmethod
    def _make_server_credentials(name: str, domains: List[str], issuer: Credentials,
                                 key_type: Any,
                                 valid_from: timedelta = timedelta(days=-1),
                                 valid_to: timedelta = timedelta(days=89),
                                 ) -> Credentials:
        name = name
        pkey = _private_key(key_type=key_type)
        subject = Ngtcp2TestCA._make_x509_name(common_name=name, parent=issuer.subject)
        csr = Ngtcp2TestCA._make_csr(subject=subject,
                                     issuer_subject=issuer.certificate.subject, pkey=pkey,
                                     valid_from_delta=valid_from, valid_until_delta=valid_to)
        csr = Ngtcp2TestCA._add_leaf_usages(csr, domains=domains, issuer=issuer)
        cert = csr.sign(private_key=issuer.private_key,
                        algorithm=hashes.SHA256(),
                        backend=default_backend())
        return Credentials(name=name, cert=cert, pkey=pkey, issuer=issuer)

    @staticmethod
    def _make_client_credentials(name: str,
                                 issuer: Credentials, email: Optional[str],
                                 key_type: Any,
                                 valid_from: timedelta = timedelta(days=-1),
                                 valid_to: timedelta = timedelta(days=89),
                                 ) -> Credentials:
        pkey = _private_key(key_type=key_type)
        subject = Ngtcp2TestCA._make_x509_name(common_name=name, parent=issuer.subject)
        csr = Ngtcp2TestCA._make_csr(subject=subject,
                                     issuer_subject=issuer.certificate.subject, pkey=pkey,
                                     valid_from_delta=valid_from, valid_until_delta=valid_to)
        csr = Ngtcp2TestCA._add_client_usages(csr, issuer=issuer, rfc82name=email)
        cert = csr.sign(private_key=issuer.private_key,
                        algorithm=hashes.SHA256(),
                        backend=default_backend())
        return Credentials(name=name, cert=cert, pkey=pkey, issuer=issuer)
