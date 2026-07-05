# Load Testing — Step 13 & 14

## Install k6

```bash
# Linux
sudo snap install k6
# or
curl https://github.com/grafana/k6/releases/download/v0.51.0/k6-v0.51.0-linux-amd64.tar.gz -L | tar xvz

# macOS
brew install k6

# Windows
choco install k6
# or download from https://k6.io/docs/get-started/installation/
```

## Run

```bash
cd load-test

# Quick smoke test (5 VUs × 15 s) — run before every deploy
k6 run smoke_test.js

# Full load test against your domain
BASE_URL=https://your-domain.com k6 run load_test.js

# Stress test with custom VUs
BASE_URL=https://your-domain.com \
ELECTION_ID_1=<real-uuid> \
ELECTION_ID_2=<real-uuid> \
  k6 run --out json=results.json load_test.js

# Quick 100-VU test
BASE_URL=https://your-domain.com k6 run --vus 100 --duration 30s load_test.js
```

## Step 14: What to Tune Based on Results

### If p99 latency is high (> 500ms)

**Thread pool too small:**
```bash
# Check current worker count in server logs
grep "Workers:" server.log

# Increase via env var override (2× CPU cores is the default)
# For a 4-core box the default is 8 workers — try 16
docker-compose down
# Edit docker-compose.yml → add env var to server services:
#   WORKER_THREADS: "16"   (requires a Config entry + EpollServer change)
docker-compose up -d
```

**Redis not warmed up:**
```bash
# Check cache hit rate in Grafana → Redis dashboard
# If hit rate < 60%, candidates/sessions aren't being cached
# Check REDIS_URL env var is set correctly in docker-compose.yml
docker-compose exec voting_server_1 env | grep REDIS
```

### If error rate spikes under load

**Queue full (503s):**
- The thread pool's `maxQueue` is `numThreads × 8` by default.
- Under extreme load you'll see "Queue full — rejected with 503" in logs.
- Tune by adding more backend instances (increase replicas in docker-compose).
- Or raise `maxQueue` in `ThreadPool` constructor call in `EpollServer.cpp`.

**Rate limiter too aggressive:**
- Default is 100 req/60s per IP. For load tests from one IP, raise this:
  ```
  RATE_LIMIT_REQUESTS=10000
  RATE_LIMIT_WINDOW_SEC=60
  ```
- In production keep it tight; for load tests from a single IP you may need
  to temporarily raise it or whitelist the test runner IP in nginx.

### If nginx is the bottleneck

```nginx
# nginx.conf tuning
worker_processes      auto;        # already set
worker_connections    8192;        # raise from 4096
keepalive_timeout     120s;        # raise if clients keep connections open
upstream voting_backend {
    least_conn;
    keepalive  64;                 # raise from 32
    ...
}
```

### If memory usage is high

```
# Reduce Redis maxmemory policy if non-session data is evicting sessions:
# allkeys-lru → volatile-lru (only evict keys with TTL — sessions have TTL)
command: redis-server --maxmemory 512mb --maxmemory-policy volatile-lru
```

## Reading k6 Output

```
✓ health 200
✓ GET /vote/candidates status 200
✓ POST /vote/check status 200 or 400

scenarios: (100.00%) 1 scenario, 2000 max VUs, 4m30s max duration
default: Up to 2000 looping VUs for 4m0s over 5 stages

✓ http_req_duration.............: avg=45ms   min=2ms   med=30ms   max=980ms  p(90)=110ms p(95)=185ms
✓ error_rate....................: 0.12%
✓ login_latency.................: avg=120ms  p(99)=450ms
```

Key numbers to watch:
- `p(95) http_req_duration` — must be < 500ms threshold
- `error_rate` — must be < 1%
- `rate_limited_429` — if non-zero, your test IP is being rate limited
- `http_req_failed` — network-level failures (timeouts, connection refused)
