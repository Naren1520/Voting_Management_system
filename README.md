# VoteStack

A production-grade online voting platform with a high-performance C++ backend, Redis caching, nginx load balancing, and a Vanilla JS frontend. Built for concurrent multi-user elections with full data isolation per account.

[![Documentation](https://img.shields.io/badge/Docs-DeepWiki-blue?style=for-the-badge&logo=gitbook&logoColor=white)](https://deepwiki.com/Naren1520/Voting_Management_system)

---

## Stack

**Backend**

![C++](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![OpenSSL](https://img.shields.io/badge/OpenSSL-721412?style=for-the-badge&logo=openssl&logoColor=white)
![libcurl](https://img.shields.io/badge/libcurl-073551?style=for-the-badge&logo=curl&logoColor=white)
![Redis](https://img.shields.io/badge/Redis-DC382D?style=for-the-badge&logo=redis&logoColor=white)
![Supabase](https://img.shields.io/badge/Supabase-3ECF8E?style=for-the-badge&logo=supabase&logoColor=white)

**Frontend**

![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-ES6+-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black)
![Netlify](https://img.shields.io/badge/Netlify-00C7B7?style=for-the-badge&logo=netlify&logoColor=white)

**Infrastructure**

![Docker](https://img.shields.io/badge/Docker-2496ED?style=for-the-badge&logo=docker&logoColor=white)
![nginx](https://img.shields.io/badge/nginx-009639?style=for-the-badge&logo=nginx&logoColor=white)
![Prometheus](https://img.shields.io/badge/Prometheus-E6522C?style=for-the-badge&logo=prometheus&logoColor=white)
![Grafana](https://img.shields.io/badge/Grafana-F46800?style=for-the-badge&logo=grafana&logoColor=white)
![Cloudflare](https://img.shields.io/badge/Cloudflare-F38020?style=for-the-badge&logo=cloudflare&logoColor=white)

---

## Table of Contents

1. [Architecture](#architecture)
2. [Features](#features)
3. [Project Structure](#project-structure)
4. [Prerequisites](#prerequisites)
5. [Local Development](#local-development)
6. [Production Deployment](#production-deployment)
7. [Environment Variables](#environment-variables)
8. [API Reference](#api-reference)
9. [Security](#security)
10. [Monitoring](#monitoring)
11. [Load Testing](#load-testing)
12. [Developer](#developer)

---

## Architecture

```
                                                             Internet
                                       │
                                       ▼
                              ┌─────────────────────┐
                              │     Cloudflare      │
                              │ CDN • DNS • DDoS    │
                              └─────────┬───────────┘
                                        │
                                        ▼
                              ┌─────────────────────┐
                              │       nginx         │
                              │ Reverse Proxy       │
                              │ HTTPS • HTTP/2      │
                              │ gzip • least_conn   │
                              └─────────┬───────────┘
                                        │
                 ┌──────────────────────┼─────────────────────┐
                 │                      │                     │
                 ▼                      ▼                     ▼
      ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
      │ VoteStack #1    │    │ VoteStack #2    │    │ VoteStack #3    │
      │ C++ Backend     │    │ C++ Backend     │    │ C++ Backend     │
      │ epoll           │    │ epoll           │    │ epoll           │
      │ Thread Pool     │    │ Thread Pool     │    │ Thread Pool     │
      └────────┬────────┘    └────────┬────────┘    └────────┬────────┘
               │                       │                       │
               └───────────────┬───────┴───────────────┬───────┘
                               │
                               ▼
                     ┌─────────────────────┐
                     │        Redis        │
                     │ Session Cache       │
                     │ Rate Limiting       │
                     │ Application Cache   │
                     └─────────┬───────────┘
                               │
                               ▼
                  ┌────────────────────────────┐
                  │  Supabase / PostgreSQL     │
                  │ Users • Elections • Votes  │
                  └────────────────────────────┘
                               │
                               ▼

                   ┌───────────────────────┐
                   │     Prometheus        │
                   │ Scrapes /metrics      │
                   │ from every instance   │
                   └──────────┬────────────┘
                              │
                              ▼
                   ┌───────────────────────┐
                   │       Grafana         │
                   │ Dashboards & Metrics  │
                   └───────────────────────┘
```

**Request flow** -- Cloudflare terminates TLS and absorbs DDoS, nginx handles HTTP/2 and routes to one of four C++ instances using `least_conn`. Each instance uses edge-triggered epoll with a thread pool. Redis handles session validation, candidate caching, and per-IP rate limiting. All vote writes go through a Postgres RPC that uses `INSERT ON CONFLICT DO NOTHING` in a single transaction, eliminating duplicate-vote races.

---

## Features

**Election management**
- Create unlimited independent elections per account
- Standard (single candidate) and multi-position ballot types
- Scheduled elections with timezone support
- Shareable per-election voting link

**Voting**
- Voter ID verification before ballot is shown
- Duplicate vote prevention enforced at the database level
- Atomic vote writes via Postgres RPC - no TOCTOU race possible
- Live results with per-candidate vote counts

**Auth**
- PBKDF2-SHA256 password hashing (100k iterations) via OpenSSL
- Session tokens stored in Redis with 24h TTL
- Per-user data isolation on every query
- Session revocation (single session or all other sessions)

**Performance**
- epoll edge-triggered I/O, non-blocking accept loop
- Bounded request queue with 503 backpressure
- Redis candidate cache (30s TTL), invalidated on mutation
- libcurl persistent connection pool, sized to thread count
- Batch voter sync (single POST, not N serial inserts)

---

## Project Structure

```
VotingSystem using cpp/
|
+-- backend/
|   \-- server/
|       +-- include/
|       |   +-- cache/
|       |   |   +-- CandidateCache.h          - 30s TTL candidate list cache
|       |   |   \-- RedisClient.h             - RESP client, pool of 32 connections
|       |   +-- controllers/
|       |   |   +-- AuthController.h          - signup, login, sessions, token validation
|       |   |   +-- CandidateController.h     - add/remove/list candidates (authed)
|       |   |   +-- ElectionController.h      - CRUD elections, schedule management
|       |   |   +-- PositionController.h      - positions for multi-ballot elections
|       |   |   +-- PublicMultiVoteController.h - multi-position public ballot
|       |   |   +-- PublicVoteController.h    - single-candidate public ballot
|       |   |   \-- VoterController.h         - register/sync/remove voters
|       |   +-- core/
|       |   |   +-- Config.h                  - env var loader (singleton)
|       |   |   +-- Logger.h                  - thread-safe logger (stdout + file)
|       |   |   +-- Metrics.h                 - Prometheus metrics (lock-free counters)
|       |   |   \-- ThreadPool.h              - bounded fixed-size thread pool
|       |   +-- db/
|       |   |   \-- SupabaseClient.h          - libcurl pool, PBKDF2, token gen
|       |   \-- net/
|       |       +-- EpollServer.h             - epoll edge-triggered server
|       |       +-- HttpRequest.h             - request parser (headers + body)
|       |       \-- HttpResponse.h            - response builder with CORS
|       +-- src/
|       |   +-- cache/
|       |   |   +-- CandidateCache.cpp
|       |   |   \-- RedisClient.cpp           - RESP protocol, buffered reads
|       |   +-- controllers/
|       |   |   +-- AuthController.cpp
|       |   |   +-- CandidateController.cpp
|       |   |   +-- ElectionController.cpp
|       |   |   +-- PositionController.cpp
|       |   |   +-- PublicMultiVoteController.cpp
|       |   |   +-- PublicVoteController.cpp
|       |   |   \-- VoterController.cpp
|       |   +-- core/
|       |   |   +-- Config.cpp
|       |   |   +-- Logger.cpp
|       |   |   \-- ThreadPool.cpp
|       |   +-- db/
|       |   |   \-- SupabaseClient.cpp
|       |   +-- net/
|       |   |   +-- EpollServer.cpp           - epoll loop, route dispatcher
|       |   |   +-- HttpRequest.cpp           - recv with 5s timeout, 1MB cap
|       |   |   \-- HttpResponse.cpp          - CORS origin validation
|       |   \-- main.cpp                      - startup: config, Redis, epoll server
|       +-- third_party/
|       |   \-- json.hpp                      - nlohmann/json single-header
|       +-- CMakeLists.txt
|       \-- Makefile
|
+-- frontend/                                 - Vanilla JS static site (Netlify)
|   +-- assets/
|   |   +-- Logo.png
|   |   +-- founder.jpg
|   |   +-- founder1.png
|   |   +-- img1.jpg  img2.jpg  img3.png
|   |   +-- img4.png  img5.jpg  img6.jpg  img7.jpg
|   |   +-- vdo1.mp4
|   |   \-- vdo2.mp4
|   +-- auth/
|   |   +-- login.html
|   |   +-- signup.html
|   |   +-- auth.css
|   |   \-- auth.js
|   +-- dashboard/
|   |   +-- index.html                        - election list (auth required)
|   |   +-- dashboard.css
|   |   \-- dashboard.js
|   +-- election/
|   |   +-- manage.html                       - single-candidate election manager
|   |   +-- manage.css
|   |   +-- manage.js
|   |   +-- manage-multi.html                 - multi-position election manager
|   |   +-- manage-multi.css
|   |   \-- manage-multi.js
|   +-- landing/
|   |   +-- index.html
|   |   +-- landing.css
|   |   \-- landing.js
|   +-- privacy/
|   |   +-- index.html
|   |   \-- privacy.js
|   +-- profile/
|   |   +-- index.html
|   |   +-- profile.css
|   |   +-- profile.js
|   |   \-- sessions/
|   |       +-- index.html                    - active sessions viewer
|   |       +-- sessions.css
|   |       \-- sessions.js
|   +-- shared/
|   |   +-- api.js                            - centralised fetch client + auth guards
|   |   +-- styles.css                        - design system (tokens, components)
|   |   +-- schedule.css
|   |   +-- schedule.js
|   |   \-- legal.css
|   +-- terms/
|   |   +-- index.html
|   |   \-- terms.js
|   +-- vote/
|   |   \-- index.html                        - public single-candidate ballot
|   +-- vote-multi/
|   |   +-- index.html                        - public multi-position ballot
|   |   +-- vote-multi.css
|   |   \-- vote-multi.js
|   +-- config.js                             - window.API_BASE (edit for local dev)
|   +-- loader.html
|   \-- netlify.toml                          - publish dir + SPA redirect rules
|
+-- docker/
|   +-- Dockerfile.server                     - multi-stage build: gcc -> debian-slim
|   \-- Dockerfile.nginx                      - nginx + self-signed cert for local dev
|
+-- nginx/
|   \-- nginx.conf                            - HTTPS, HTTP/2, gzip, least_conn upstream
|
+-- monitoring/
|   +-- prometheus.yml                        - scrape config: backend, Redis, nginx, node
|   +-- alert_rules.yml                       - alerts: instance down, latency, errors
|   \-- grafana/
|       +-- dashboards/
|       |   \-- votestack.json                - pre-built overview dashboard
|       \-- provisioning/
|           +-- dashboards/
|           |   \-- dashboards.yml            - auto-load dashboards from disk
|           \-- datasources/
|               \-- prometheus.yml            - auto-connect Prometheus datasource
|
+-- load-test/
|   +-- smoke_test.js                         - 5 VUs x 15s, run before every deploy
|   +-- load_test.js                          - staged ramp: 100 -> 500 -> 2000 VUs
|   \-- README.md                             - benchmark tuning guide
|
+-- docker-compose.yml                        - full stack orchestration
+-- .env.example                              - all env vars with descriptions
+-- .dockerignore
+-- .gitignore
\-- README.md
```

---

## Prerequisites

| Tool        | Version | Purpose                          |
|-------------|---------|----------------------------------|
| Docker      | 24+     | Build and run all services       |
| Docker Compose | v2+  | Orchestrate the full stack       |
| k6          | any     | Load testing (optional)          |
| GCC / G++   | 12+     | Local build only (no Docker)     |
| Python      | 3.x     | Serve frontend locally           |

---

## Local Development

### 1 - Clone and configure

```bash
git clone <repo-url>
cd "VotingSystem using cpp"
cp .env.example .env
# Edit .env — fill in SUPABASE_URL and SUPABASE_KEY
```

### 2 - Start the full stack

```bash
docker compose up -d --build
```

| Service    | URL                       |
|------------|---------------------------|
| API        | http://localhost:8080     |
| Grafana    | http://localhost:3001     |
| Prometheus | http://localhost:9090     |

### 3 - Serve the frontend

```bash
cd frontend
python -m http.server 3000
```

Open `http://localhost:3000/landing/index.html`

Edit `frontend/config.js` to point at the local backend:

```js
window.API_BASE = 'http://localhost:8080';
```

### 4 - Build the backend only (no Docker)

```bash
cd backend/server
make            # outputs to bin/voting_server
make run        # build + start with env vars from shell
```

---

## Production Deployment

### Render — Backend

Render reads `render.yaml` from the repo root and builds the `Dockerfile` automatically.

**Step 1 — Connect the repo**

1. Go to [render.com](https://render.com) and create a new account or sign in.
2. Click **New > Blueprint** and connect your GitHub repository.
3. Render detects `render.yaml` and shows two services to create:
   - `voting-backend` (web service)
   - `voting-redis` (managed Redis)
4. Click **Apply** to create both.

**Step 2 — Set secret environment variables**

In the Render dashboard, open the `voting-backend` service and go to **Environment**.
Add these two variables manually (they are marked `sync: false` in `render.yaml`):

| Key              | Value                                      |
|------------------|--------------------------------------------|
| `SUPABASE_URL`   | your Supabase project URL                  |
| `SUPABASE_KEY`   | your Supabase anon or service-role key     |
| `ALLOWED_ORIGINS`| your Netlify site URL, e.g. `https://votestack.netlify.app` |

All other variables (`REDIS_URL`, `RATE_LIMIT_REQUESTS`, etc.) are already set in `render.yaml`.
`REDIS_URL` is automatically wired from the managed Redis service — no manual copy needed.

**Step 3 — Deploy**

Click **Manual Deploy > Deploy latest commit** or just push to `main`.
The build takes 2-4 minutes. Watch the logs in the Render dashboard.

When the status shows **Live**, copy the service URL (e.g. `https://voting-backend.onrender.com`).

**Step 4 — Update frontend config**

Open `frontend/config.js` and set your Render URL:

```js
var PRODUCTION_URL = 'https://voting-backend.onrender.com';
```

Commit and push. Netlify will redeploy automatically.

**Health check**

Render pings `GET /health` every 30 seconds. The endpoint returns:

```json
{ "status": "ok" }
```

If it fails 3 times the service is restarted automatically.

> **Free tier note** — Render free web services spin down after 15 minutes of inactivity
> and take ~30 seconds to cold start on the next request. Upgrade to the Starter plan
> ($7/month) to keep it always on.

---

### Netlify — Frontend

**Step 1 — Connect the repo**

1. Go to [netlify.com](https://netlify.com) and sign in.
2. Click **Add new site > Import an existing project**.
3. Choose GitHub and select your repository.

**Step 2 — Configure build settings**

Set these in the Netlify UI:

| Setting           | Value        |
|-------------------|--------------|
| Base directory    | `frontend`   |
| Build command     | _(leave empty — no build step)_ |
| Publish directory | `frontend`   |

> If Netlify auto-detects a framework, clear any auto-filled build command.
> This is a plain static site.

**Step 3 — Deploy**

Click **Deploy site**. It finishes in under 30 seconds.

Netlify reads `frontend/netlify.toml` automatically for redirect rules and security headers — no extra dashboard configuration needed.

**Step 4 — Set your custom domain (optional)**

In the Netlify dashboard go to **Domain settings** and add your domain.
Netlify provisions a free TLS certificate via Let's Encrypt automatically.

---

### Connecting the two services

After both are deployed, make sure:

1. `frontend/config.js` has your Render URL in `PRODUCTION_URL`.
2. The `ALLOWED_ORIGINS` env var on Render contains your Netlify site URL.

These two values are the only link between the frontend and backend.

```
Netlify (frontend)          Render (backend)
https://votestack.netlify.app  -->  https://voting-backend.onrender.com
         |                                      |
         |   config.js: PRODUCTION_URL          |   ALLOWED_ORIGINS
         +--------------------------------------+
```

---

### Checking the deployment

```bash
# Backend health check
curl https://voting-backend.onrender.com/health
# expected: {"status":"ok"}

# Auth endpoint
curl -X POST https://voting-backend.onrender.com/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"test@example.com","password":"wrong"}' 
# expected: {"success":false,"message":"Invalid email or password"}
```

---

## Environment Variables

| Variable               | Required | Default                        | Description                          |
|------------------------|----------|--------------------------------|--------------------------------------|
| `SUPABASE_URL`         | yes      | -                              | Supabase project REST URL            |
| `SUPABASE_KEY`         | yes      | -                              | Supabase anon or service role key    |
| `REDIS_URL`            | no       | `redis://127.0.0.1:6379`       | Redis connection string              |
| `PORT`                 | no       | `8080`                         | Server listen port                   |
| `RATE_LIMIT_REQUESTS`  | no       | `100`                          | Max requests per IP per window       |
| `RATE_LIMIT_WINDOW_SEC`| no       | `60`                           | Rate limit window in seconds         |
| `ALLOWED_ORIGINS`      | no       | Netlify + localhost            | Comma-separated CORS origins         |
| `LOG_FILE`             | no       | `server.log`                   | Log file path inside container       |
| `GRAFANA_USER`         | no       | `admin`                        | Grafana admin username               |
| `GRAFANA_PASSWORD`     | no       | -                              | Grafana admin password               |

---

## API Reference

**Base URL** -- `https://your-domain.com`

All protected routes require:

```
Authorization: Bearer <token>
```

### Auth

| Method | Endpoint                    | Auth | Description                    |
|--------|-----------------------------|------|--------------------------------|
| POST   | `/api/auth/signup`          | -    | Register new account           |
| POST   | `/api/auth/login`           | -    | Authenticate, returns token    |
| POST   | `/api/auth/logout`          | yes  | Revoke current session         |
| GET    | `/api/auth/ping`            | yes  | Validate token                 |
| POST   | `/api/auth/change-password` | yes  | Change password                |
| GET    | `/api/auth/sessions`        | yes  | List active sessions           |
| DELETE | `/api/auth/sessions`        | yes  | Revoke all other sessions      |
| DELETE | `/api/auth/sessions/:id`    | yes  | Revoke specific session        |

### Elections

| Method | Endpoint                              | Auth | Description                    |
|--------|---------------------------------------|------|--------------------------------|
| GET    | `/api/elections`                      | yes  | List elections for user        |
| POST   | `/api/elections`                      | yes  | Create election                |
| GET    | `/api/elections/:id`                  | yes  | Get election                   |
| DELETE | `/api/elections/:id`                  | yes  | Delete election and all data   |
| PATCH  | `/api/elections/:id/schedule`         | yes  | Update schedule settings       |
| GET    | `/api/elections/:id/candidates`       | yes  | List candidates                |
| POST   | `/api/elections/:id/candidates`       | yes  | Add candidate                  |
| DELETE | `/api/elections/:id/candidates`       | yes  | Remove candidate               |
| GET    | `/api/elections/:id/voters`           | yes  | List registered voters         |
| POST   | `/api/elections/:id/voters`           | yes  | Register voter                 |
| DELETE | `/api/elections/:id/voters`           | yes  | Remove voter                   |
| POST   | `/api/elections/:id/voters/sync`      | yes  | Batch-replace voter list       |
| GET    | `/api/elections/:id/positions`        | yes  | List positions (multi-ballot)  |
| POST   | `/api/elections/:id/positions`        | yes  | Add position                   |
| DELETE | `/api/elections/:id/positions/:posId` | yes  | Remove position                |

### Public Voting (no auth)

| Method | Endpoint                        | Description                          |
|--------|---------------------------------|--------------------------------------|
| GET    | `/api/vote/:id/candidates`      | Candidate list for ballot            |
| GET    | `/api/vote/:id/info`            | Election title and active status     |
| POST   | `/api/vote/:id/check`           | Verify voter ID, check if voted      |
| POST   | `/api/vote/:id/cast`            | Cast vote (atomic, duplicate-safe)   |
| GET    | `/api/vote/:id/results`         | Live results                         |
| GET    | `/api/multi-vote/:id/positions` | Full ballot for multi-position       |
| POST   | `/api/multi-vote/:id/check`     | Check voter for multi-position       |
| POST   | `/api/multi-vote/:id/cast`      | Cast all position votes atomically   |
| GET    | `/api/multi-vote/:id/results`   | Multi-position live results          |

### Example responses

```json
POST /api/auth/login
{
  "success": true,
  "token": "a3f9...",
  "user": { "id": "uuid", "name": "Naren", "email": "n@example.com" }
}

POST /api/vote/:id/cast
{
  "success": true,
  "message": "Vote cast successfully for Alice"
}

GET /api/vote/:id/results
{
  "success": true,
  "total_votes": 42,
  "candidates": [
    { "name": "Alice", "votes": 28 },
    { "name": "Bob",   "votes": 14 }
  ]
}
```

---

## Security

| Concern                   | Implementation                                              |
|---------------------------|-------------------------------------------------------------|
| Password hashing          | PBKDF2-SHA256, 100k iterations, random salt per password    |
| Password comparison       | Constant-time via `CRYPTO_memcmp`                           |
| Session tokens            | 32 random bytes via `RAND_bytes`, stored in Redis + Supabase|
| Duplicate vote prevention | `INSERT ON CONFLICT DO NOTHING` in single DB transaction    |
| Rate limiting             | Redis INCR + EXPIRE per IP, 100 req / 60s default          |
| CORS                      | Origin validated against `ALLOWED_ORIGINS` allowlist        |
| Auth guards               | Every protected route validates token before any DB access  |
| Data isolation            | Every query scoped to authenticated `user_id`               |
| XSS prevention            | `escHtml()` sanitiser on all user-supplied output           |
| Request size cap          | 1 MB body limit, 5s receive timeout per connection          |
| TLS                       | nginx terminates HTTPS, TLSv1.2/1.3 only, HSTS enabled     |

---

## Monitoring

Prometheus scrapes each backend instance, Redis, nginx, and the host every 15 seconds.

Access Grafana at `http://your-server:3001` (restrict this port to your IP via firewall).

The pre-loaded **VoteStack Overview** dashboard shows:

- Requests/sec per instance
- Latency histogram (p50 / p95 / p99)
- 5xx error rate
- Active connections per instance
- Redis memory usage and commands/sec
- nginx active and waiting connections
- Host CPU and memory gauges

Alert rules are defined in `monitoring/alert_rules.yml` and fire on:

- Any backend instance unreachable for more than 1 minute
- 5xx error rate above 5% for 2 minutes
- p99 latency above 2 seconds for 3 minutes
- Redis unreachable or above 80% memory

---

## Load Testing

Requires [k6](https://k6.io/docs/get-started/installation/).

```bash
# Smoke test -- run before every deploy (5 VUs, 15s)
BASE_URL=https://your-domain.com k6 run load-test/smoke_test.js

# Full staged load test (ramps to 2000 VUs over ~4 minutes)
BASE_URL=https://your-domain.com \
ELECTION_ID_1=<uuid> \
ELECTION_ID_2=<uuid> \
  k6 run load-test/load_test.js
```

Default thresholds: p95 latency < 500ms, error rate < 1%.

See `load-test/README.md` for tuning guidance on thread pool size, Redis memory, and nginx worker connections based on benchmark results.

---

## Developer

**Naren S J**  
narensonu1520@gmail.com

[Project Documentation](https://deepwiki.com/Naren1520/Voting_Management_system)
