var exceptions = [];
try { location.href = 1; } catch(e) { exceptions.push('href'); }
try { location.protocol = 1; } catch(e) { exceptions.push('protocol'); }
try { location.host = 1; } catch(e) { exceptions.push('host'); }
try { location.hostname = 1; } catch(e) { exceptions.push('hostname');}
try { location.port = 1; } catch(e) { exceptions.push('port'); }
try { location.pathname = 1; } catch(e) { exceptions.push('pathname'); }
try { location.search = 1; } catch(e) { exceptions.push('search'); }
try { location.hash = 1; } catch(e) { exceptions.push('hash'); }

postMessage([null, location.href, location.protocol, location.host,
             location.hostname, location.port, location.pathname,
             location.search, location.hash, exceptions]);