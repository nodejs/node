select
  pid,
  max(c.ts) as last_rss,
  p.end_ts as process_end
from counter c
join process_counter_track t on c.track_id = t.id
join process p using(upid)
group by upid