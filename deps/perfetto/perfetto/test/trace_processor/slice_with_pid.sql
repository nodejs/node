select s.name, dur, tid, pid
from slice s
join thread_track t on s.track_id = t.id
join thread using(utid)
left join process using(upid);