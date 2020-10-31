SELECT
  ts,
  tid,
  CAST(SUBSTR(slices.name, 18) AS int) AS cpu,
  CAST(key AS int) AS freq,
  int_value AS time_ms
FROM slices
JOIN thread_track ON (slices.track_id = thread_track.id)
JOIN thread USING (utid)
JOIN args USING (arg_set_id)
WHERE slices.name LIKE "time_in_state.%"
ORDER BY ts, tid, cpu, key;
