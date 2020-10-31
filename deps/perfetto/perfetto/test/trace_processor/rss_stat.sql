SELECT c.ts, t.name, p.pid, p.name, c.value
FROM counter c
JOIN process_counter_track t ON c.track_id = t.id
JOIN process p USING (upid)
ORDER BY ts
