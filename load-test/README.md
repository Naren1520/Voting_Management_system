# VoteStack Load Tests

## Install k6

Windows (via winget):
```
winget install k6 --source winget
```
Or download from: https://k6.io/docs/get-started/installation/

---

## Smoke Test - run before every deploy

5 virtual users, 20 seconds. Confirms health, metrics, auth validation, and public vote endpoints work.

```bash
k6 run load-test/smoke_test.js
```

Expected output on Render free tier:
```
✓ health: status 200
✓ health: body has status
✓ metrics: status 200
✓ vote/candidates: no 5xx
✓ signup validation: 400

checks.........................: 100%
http_req_duration..............: avg=250ms p(95)=800ms
```

---

## Load Test - staged ramp

Ramps from 10 → 50 → 100 virtual users. Tests auth flow, public voting, and health endpoints.

```bash
# Basic (uses defaults)
k6 run load-test/load_test.js

# With your real election ID for vote testing
k6 run --env ELECTION_ID=<your-election-uuid> load-test/load_test.js

# Save results to JSON for analysis
k6 run --out json=load-test/results.json load-test/load_test.js
```

---

## Quick single-stage test

```bash
# 10 users for 30 seconds
k6 run --vus 10 --duration 30s load-test/load_test.js
```

---

## What to look for

| Metric | Good (free tier) | Needs attention |
|--------|-----------------|-----------------|
| `http_req_duration p95` | < 2s | > 5s |
| `error_rate` | < 1% | > 5% |
| `login_latency p99` | < 3s | > 8s |
| `rate_limited_429` | 0 | > 0 (hitting rate limit) |

---

## Get your election ID

1. Log in to https://votestack.netlify.app
2. Create an election and add at least one candidate
3. The election ID is in the URL when you open it: `/election/manage.html?id=<uuid>`
4. Pass it as `--env ELECTION_ID=<uuid>`

---

## Interpreting results on Render free tier

- Render free tier has 0.1 vCPU and 512MB RAM
- Expect p95 latency of 1-3s under load (normal for free tier)
- Login is slower (~1-2s) due to PBKDF2 100k iterations being CPU-intensive
- Rate limit is 100 req/IP/60s - k6 runs from one IP so you may hit this at high VUs
- Cold start adds ~30s to first request after inactivity
