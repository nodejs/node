SELECT ts, instant.name, process.pid, process.name
FROM instant JOIN process ON instant.ref = process.upid
WHERE instant.ref_type = 'upid';
