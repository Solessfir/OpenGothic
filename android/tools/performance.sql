-- Vulkan presentation cadence, not confirmed display scanout or GPU execution time.
-- Driver trace markers are device-dependent.
-- Empty results mean the marker was unavailable, not that the game ran at zero FPS.
WITH frames AS (
  SELECT process.upid, thread.utid, slice.ts,
         lead(slice.ts) OVER (PARTITION BY thread.utid ORDER BY slice.ts) - slice.ts AS dt
  FROM slice
  JOIN thread_track ON slice.track_id = thread_track.id
  JOIN thread USING (utid)
  JOIN process USING (upid)
  WHERE process.name = 'org.opengothic.app' AND slice.name = 'QueuePresentKHR'
)
SELECT upid, utid, count(dt) AS intervals,
       round(1e9 / avg(dt), 2) AS presentation_fps,
       round(avg(dt) / 1e6, 2) AS average_ms,
       round(percentile(dt, 50) / 1e6, 2) AS median_ms,
       round(percentile(dt, 95) / 1e6, 2) AS p95_ms,
       round(percentile(dt, 99) / 1e6, 2) AS p99_ms,
       round(max(dt) / 1e6, 2) AS worst_ms
FROM frames WHERE dt > 0 GROUP BY upid, utid;

-- CPU time per thread excludes sleeping and scheduler waits.
SELECT process.pid, thread.tid, thread.name,
       round(sum(sched.dur) / 1e9, 3) AS cpu_seconds
FROM sched
JOIN thread USING (utid)
JOIN process USING (upid)
WHERE process.name = 'org.opengothic.app' AND sched.dur > 0
GROUP BY thread.utid ORDER BY cpu_seconds DESC LIMIT 15;

-- These slices can overlap or nest; never add them to estimate total frame time.
-- GPU-completion waits include queued work, not just execution of one render pass.
SELECT thread.tid,
       CASE WHEN slice.name GLOB 'waiting for GPU completion*'
            THEN 'waiting for GPU completion' ELSE slice.name END AS marker,
       count(*) AS samples,
       round(avg(slice.dur) / 1e6, 3) AS average_ms,
       round(max(slice.dur) / 1e6, 3) AS worst_ms
FROM slice
JOIN thread_track ON slice.track_id = thread_track.id
JOIN thread USING (utid)
JOIN process USING (upid)
WHERE process.name = 'org.opengothic.app' AND slice.dur > 0
  AND (slice.name IN ('QueuePresentKHR', 'QueueSubmit', 'AcquireNextImageKHR',
                     'AllocateCommandBuffers', 'waitForever')
       OR slice.name GLOB 'waiting for GPU completion*')
GROUP BY thread.utid, marker ORDER BY average_ms DESC;

-- Check capture quality before interpreting the numbers.
SELECT name, value FROM stats WHERE severity != 'info' AND value > 0;
