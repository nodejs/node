SELECT t.name, c.ts, c.value
FROM counter c
JOIN track t ON c.track_id = t.id
WHERE t.name LIKE 'mem.ion%';
