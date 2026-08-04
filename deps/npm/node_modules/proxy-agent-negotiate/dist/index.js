export function createNegotiateAuth() {
    return async ({ response, scheme }) => {
        if (scheme.toLowerCase() !== 'negotiate') {
            throw new Error(`Expected Negotiate scheme but got "${scheme}"`);
        }
        let kerberos;
        try {
            kerberos = await import('kerberos');
        }
        catch {
            throw new Error('The "kerberos" package is required for Negotiate proxy authentication. ' +
                'Install it with: npm install kerberos');
        }
        const proxyAuthenticate = response.headers['proxy-authenticate'] || '';
        const challengeHeader = Array.isArray(proxyAuthenticate)
            ? proxyAuthenticate[0]
            : proxyAuthenticate;
        const serverToken = typeof challengeHeader === 'string' && challengeHeader.includes(' ')
            ? challengeHeader.split(' ').slice(1).join(' ')
            : undefined;
        const client = await kerberos.initializeClient('HTTP@proxy', {
            mechOID: kerberos.GSS_MECH_OID_SPNEGO,
        });
        const token = await client.step(serverToken || '');
        if (!token) {
            throw new Error('Kerberos client.step() returned no token');
        }
        return {
            headers: {
                'Proxy-Authorization': `Negotiate ${token}`,
            },
        };
    };
}
//# sourceMappingURL=index.js.map