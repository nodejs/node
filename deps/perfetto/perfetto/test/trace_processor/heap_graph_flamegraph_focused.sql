SELECT
  id,
  depth,
  name,
  count,
  cumulative_count,
  size,
  cumulative_size,
  parent_id
FROM experimental_flamegraph(
  (select max(graph_sample_ts) from heap_graph_object),
  (select max(upid) from heap_graph_object),
  'graph')
WHERE focus_str = 'left'
LIMIT 10
