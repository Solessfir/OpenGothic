-- Wall time includes waits and nested scopes; do not sum overlapping regions.
SELECT thread.tid, slice.name, count(*) AS calls,
       round(sum(slice.dur)/1e6,3) AS total_ms,
       round(avg(slice.dur)/1e6,3) AS mean_ms,
       round(percentile(slice.dur,95)/1e6,3) AS p95_ms,
       round(max(slice.dur)/1e6,3) AS worst_ms
FROM slice
JOIN thread_track ON slice.track_id=thread_track.id
JOIN thread USING (utid)
JOIN process USING (upid)
WHERE process.name='org.opengothic.app' AND slice.dur>0
  AND (slice.name GLOB 'OpenGothic::*' OR slice.name GLOB 'Tempest::*')
GROUP BY thread.utid,slice.name ORDER BY total_ms DESC;

-- Measure on-CPU time within each region, excluding scheduling and sleep waits.
-- Nested scopes still overlap; these are inclusive, not additive costs.
SELECT thread.tid, slice.name,
       round(sum(min(slice.ts+slice.dur,sched.ts+sched.dur)-max(slice.ts,sched.ts))/1e6,3) AS cpu_ms
FROM slice
JOIN thread_track ON slice.track_id=thread_track.id
JOIN thread USING (utid)
JOIN process USING (upid)
JOIN sched ON sched.utid=thread.utid
  AND sched.ts<slice.ts+slice.dur AND sched.ts+sched.dur>slice.ts
WHERE process.name='org.opengothic.app' AND slice.dur>0 AND sched.dur>0
  AND (slice.name GLOB 'OpenGothic::*' OR slice.name GLOB 'Tempest::*')
GROUP BY thread.utid,slice.name ORDER BY cpu_ms DESC;
