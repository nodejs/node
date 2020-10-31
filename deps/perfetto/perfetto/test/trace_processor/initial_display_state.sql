SELECT t.name,
       c.ts,
       c.value
FROM counter_track t
    JOIN counter c ON t.id = c.track_id
WHERE t.name = 'ScreenState';
