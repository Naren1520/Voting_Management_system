# VoteStack

A production-grade online voting platform with a high-performance C++ backend, biometric face verification, Redis caching, nginx load balancing, and a Vanilla JS frontend. Built for concurrent multi-user elections with full data isolation per account.

[![Documentation](https://img.shields.io/badge/Docs-DeepWiki-blue?style=for-the-badge&logo=gitbook&logoColor=white)](https://deepwiki.com/Naren1520/Voting_Management_system)

---

## Stack

**Backend**

![C++](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![OpenSSL](https://img.shields.io/badge/OpenSSL-721412?style=for-the-badge&logo=openssl&logoColor=white)
![libcurl](https://img.shields.io/badge/libcurl-073551?style=for-the-badge&logo=curl&logoColor=white)
![Redis](https://img.shields.io/badge/Redis-DC382D?style=for-the-badge&logo=redis&logoColor=white)
![Supabase](https://img.shields.io/badge/Supabase-3ECF8E?style=for-the-badge&logo=supabase&logoColor=white)

**Face Verification Service**

![Python](https://img.shields.io/badge/Python-3.11-3776AB?style=for-the-badge&logo=python&logoColor=white)
![FastAPI](https://img.shields.io/badge/FastAPI-009688?style=for-the-badge&logo=fastapi&logoColor=white)
![OpenCV](https://img.shields.io/badge/OpenCV-5C3EE8?style=for-the-badge&logo=opencv&logoColor=white)
![InsightFace](https://img.shields.io/badge/InsightFace-ArcFace-FF6B35?style=for-the-badge&logo=python&logoColor=white)
![MediaPipe](https://img.shields.io/badge/MediaPipe-Liveness-0097A7?style=for-the-badge&logo=google&logoColor=white)
![ONNX](https://img.shields.io/badge/ONNX_Runtime-CPU-717272?style=for-the-badge&logo=onnx&logoColor=white)
![Modal](https://img.shields.io/badge/Modal-Serverless-6366F1?style=for-the-badge&logo=modal&logoColor=white)

**Frontend**

![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-ES6+-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black)
![WebRTC](https://img.shields.io/badge/WebRTC-Camera-333333?style=for-the-badge&logo=webrtc&logoColor=white)
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
                               │                       │
               ┌───────────────┘                       │
               │                                       │
               ▼                                       ▼
   ┌─────────────────────┐               ┌─────────────────────────┐
   │        Redis        │               │   Modal.com (Python)    │
   │ Session Cache       │               │   Face Service          │
   │ Rate Limiting       │               │   InsightFace ArcFace   │
   │ Candidate Cache     │               │   MediaPipe Liveness    │
   └─────────────────────┘               │   ONNX Runtime (CPU)   │
                                         └────────────┬────────────┘
               │                                      │
               └──────────────────┬───────────────────┘
                                  │
                                  ▼
                   ┌──────────────────────────────┐
                   │   Supabase / PostgreSQL      │
                   │ Users • Elections • Votes    │
                   │ Sessions • Voters            │
                   │ Positions • Embeddings       │
                   └──────────────────────────────┘
                                  │
                                  ▼
                   ┌──────────────────────────────┐
                   │         Prometheus           │
                   │  Scrapes GET /metrics        │
                   │  every 10 s from backend     │
                   └──────────────┬───────────────┘
                                  │
                                  ▼
                   ┌──────────────────────────────┐
                   │           Grafana            │
                   │  Dashboards & Alerts         │
                   └──────────────────────────────┘
```

```
┌─────────────────────────────────────────────────────────────────┐
│  Face Verification Flow (when enabled per election)             │
│                                                                 │
│  Browser (WebRTC)                                               │
│    │  25 frames captured                                        │
│    │  MediaPipe JS → liveness check (blink / head move)         │
│    │  Best frame selected                                       │
│    ▼                                                            │
│  C++ Backend                                                    │
│    │  Fetches stored embedding from Supabase (DB owner)         │
│    │  Forwards: { best_frame, stored_embeddings }               │
│    ▼                                                            │
│  Modal Python Service  (stateless - no DB access)               │
│    │  InsightFace → live 512-dim face embedding                 │
│    │  Cosine similarity vs stored embeddings                    │
│    │  score ≥ threshold (0.82) ?                                │
│    ▼                                                            │
│  C++ Backend → { verified, score } → Frontend                   │
│      Verified  →  Ballot shown                                  │
│      Failed    →  Retry prompt                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Request flow** -- Cloudflare terminates TLS and absorbs DDoS, nginx handles HTTP/2 and routes to one of four C++ instances using `least_conn`. Each instance uses edge-triggered epoll with a thread pool. Redis handles session validation, candidate caching, and per-IP rate limiting. All vote writes go through a Postgres RPC that uses `INSERT ON CONFLICT DO NOTHING` in a single transaction, eliminating duplicate-vote races.

**Face verification flow** -- When enabled for an election: browser captures 25 frames → MediaPipe liveness check (blink/head movement) → best frame selected → sent to C++ backend → C++ fetches stored embeddings from Supabase → forwards frame + embeddings to Modal Python service → InsightFace generates live embedding → cosine similarity comparison → result returned. The Python service is stateless and never accesses the database directly.

---

## Features

**Election management**
- Create unlimited independent elections per account
- Standard (single candidate) and multi-position ballot types
- Scheduled elections with timezone support
- Shareable per-election voting link and Election ID
- Join page - voters enter Election ID to access their ballot

**Biometric face verification**
- Admin enrolls voter photos via file upload or live webcam capture (3 angles: front/left/right)
- InsightFace ArcFace model generates 512-dim face embeddings at enrollment time
- MediaPipe FaceMesh liveness detection (blink / head-movement) prevents photo spoofing
- Cosine similarity comparison at voting time - configurable threshold (default 0.82)
- Embeddings stored encrypted in DB; raw photos are never persisted
- Stateless Python microservice on Modal.com - C++ backend owns all DB access
- Graceful re-enrollment - admin can update photos at any time

**Voting**
- Face verification required before ballot is shown
- Voter ID verification before face step
- Duplicate vote prevention enforced at the database level
- Atomic vote writes via `INSERT ON CONFLICT DO NOTHING` - no TOCTOU race possible
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
|       |   |   +-- FaceController.h          - face enroll/verify proxy to Python service
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
|       |   |   +-- FaceController.cpp        - direct libcurl calls to face service
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
+-- face-service/                             - Python face verification microservice (Modal)
|   +-- app/
|   |   +-- __init__.py
|   |   +-- config.py                         - configurable threshold, secrets
|   |   +-- face_engine.py                    - InsightFace + ONNX + cosine similarity
|   |   +-- liveness.py                       - MediaPipe blink/head-move detection
|   |   +-- models.py                         - Pydantic request/response schemas
|   |   +-- security.py                       - shared-secret API auth
|   |   \-- routes/
|   |       +-- embedding.py                  - POST /generate-embedding (enroll)
|   |       +-- health.py                     - GET /health
|   |       \-- verify.py                     - POST /verify (voting)
|   +-- main.py                               - FastAPI app entry point
|   +-- modal_app.py                          - Modal.com deployment wrapper
|   +-- requirements.txt
|   +-- Dockerfile
|   +-- .env.example
|   \-- README.md                             - setup, hosting, schema, API docs
|
+-- frontend/                                 - Vanilla JS static site (Netlify)
|   +-- assets/
|   |   +-- Logo.png
|   |   +-- img1..img7, vdo1..vdo2            - landing page media
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
|   +-- face-enroll/                          - admin face enrollment (upload + camera)
|   |   +-- index.html
|   |   +-- face-enroll.css
|   |   \-- face-enroll.js
|   +-- join/                                 - voter entry page (enter election ID)
|   |   +-- index.html
|   |   +-- join.css
|   |   \-- join.js
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
|   |   +-- face-capture.js                   - browser liveness + best-frame selection
|   |   +-- styles.css                        - design system (tokens, components)
|   |   +-- schedule.css
|   |   +-- schedule.js
|   |   \-- legal.css
|   +-- terms/
|   |   +-- index.html
|   |   \-- terms.js
|   +-- vote/                                 - public single-candidate ballot
|   |   +-- index.html
|   |   +-- vote.css                          - imports vote-multi.css (shared design)
|   |   \-- vote.js
|   +-- vote-multi/                           - public multi-position ballot
|   |   +-- index.html
|   |   +-- vote-multi.css
|   |   \-- vote-multi.js
|   +-- config.js                             - window.API_BASE (auto-detects local/prod)
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
|   +-- prometheus.yml                        - scrapes Render backend + local services
|   +-- alert_rules.yml                       - alerts: instance down, latency, errors
|   +-- docker-compose.monitoring.yml         - local Prometheus + Grafana stack
|   \-- grafana/
|       +-- dashboards/
|       |   \-- votestack.json                - pre-built VoteStack overview dashboard
|       \-- provisioning/
|           +-- dashboards/
|           |   \-- dashboards.yml
|           \-- datasources/
|               \-- prometheus.yml
|
+-- load-test/
|   +-- smoke_test.js                         - 5 VUs x 20s, run before every deploy
|   +-- load_test.js                          - staged ramp: 10 -> 50 -> 100 VUs
|   \-- README.md                             - k6 install + run instructions
|
+-- docker-compose.yml                        - full stack orchestration (self-hosted)
+-- supabase_schema.sql                       - full DB schema + GRANT statements
+-- render.yaml                               - Render deployment config
+-- .env.example                              - all env vars documented
+-- .dockerignore
+-- .gitignore
\-- README.md
```

---

## Prerequisites

| Tool           | Version | Purpose                                        |
|----------------|---------|------------------------------------------------|
| Docker         | 24+     | Build and run all services                     |
| Docker Compose | v2+     | Orchestrate the full stack                     |
| k6             | any     | Load testing (optional)                        |
| GCC / G++      | 12+     | Local C++ build only (no Docker)               |
| Python         | 3.11+   | Serve frontend locally + deploy face service   |
| pip / modal    | latest  | Deploy face-service to Modal.com               |
| Supabase       | -       | Free PostgreSQL DB (supabase.com)              |
| Redis Cloud    | -       | Free Redis instance (redis.io/try-free)        |
| Modal.com      | -       | Free serverless Python for face service        |

---

## Local Development

### 1 - Clone and configure

```bash
git clone <repo-url>
cd "VotingSystem using cpp"
cp .env.example .env
# Edit .env - fill in SUPABASE_URL and SUPABASE_KEY
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

### 4 - Deploy the face service (optional)

```bash
pip install modal
python -m modal setup          # authenticate with modal.com
cd face-service
python -m modal secret create votestack-face-secrets \
  FACE_API_SECRET="your-secret" FACE_THRESHOLD="0.82" \
  INSIGHTFACE_MODEL="buffalo_sc" \
  FACE_ALLOWED_ORIGINS="http://localhost:8080"
modal deploy modal_app.py
```

Add `FACE_SERVICE_URL` and `FACE_API_SECRET` to your backend's `.env`.

### 5 - Build the backend only (no Docker)

```bash
cd backend/server
make            # outputs to bin/voting_server
make run        # build + start with env vars from shell
```

---

## Production Deployment

### Render - Backend

Render reads `render.yaml` from the repo root and builds the `Dockerfile` automatically.

**Step 1 - Connect the repo**

1. Go to [render.com](https://render.com) and create a new account or sign in.
2. Click **New > Blueprint** and connect your GitHub repository.
3. Render detects `render.yaml` and shows two services to create:
   - `voting-backend` (web service)
   - `voting-redis` (managed Redis)
4. Click **Apply** to create both.

**Step 2 - Set secret environment variables**

In the Render dashboard, open the `voting-backend` service and go to **Environment**.
Add these two variables manually (they are marked `sync: false` in `render.yaml`):

| Key              | Value                                      |
|------------------|--------------------------------------------|
| `SUPABASE_URL`   | your Supabase project URL                  |
| `SUPABASE_KEY`   | your Supabase anon or service-role key     |
| `ALLOWED_ORIGINS`| your Netlify site URL, e.g. `https://votestack.netlify.app` |

All other variables (`REDIS_URL`, `RATE_LIMIT_REQUESTS`, etc.) are already set in `render.yaml`.
`REDIS_URL` is automatically wired from the managed Redis service - no manual copy needed.

**Step 3 - Deploy**

Click **Manual Deploy > Deploy latest commit** or just push to `main`.
The build takes 2-4 minutes. Watch the logs in the Render dashboard.

When the status shows **Live**, copy the service URL (e.g. `https://voting-backend.onrender.com`).

**Step 4 - Update frontend config**

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

> **Free tier note** - Render free web services spin down after 15 minutes of inactivity
> and take ~30 seconds to cold start on the next request. Upgrade to the Starter plan
> ($7/month) to keep it always on.

---

### Netlify - Frontend

**Step 1 - Connect the repo**

1. Go to [netlify.com](https://netlify.com) and sign in.
2. Click **Add new site > Import an existing project**.
3. Choose GitHub and select your repository.

**Step 2 - Configure build settings**

Set these in the Netlify UI:

| Setting           | Value        |
|-------------------|--------------|
| Base directory    | `frontend`   |
| Build command     | _(leave empty - no build step)_ |
| Publish directory | `frontend`   |

> If Netlify auto-detects a framework, clear any auto-filled build command.
> This is a plain static site.

**Step 3 - Deploy**

Click **Deploy site**. It finishes in under 30 seconds.

Netlify reads `frontend/netlify.toml` automatically for redirect rules and security headers - no extra dashboard configuration needed.

**Step 4 - Set your custom domain (optional)**

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

### C++ Backend (Render)

| Variable               | Required | Default                        | Description                               |
|------------------------|----------|--------------------------------|-------------------------------------------|
| `SUPABASE_URL`         | yes      | -                              | Supabase project REST URL                 |
| `SUPABASE_KEY`         | yes      | -                              | Supabase **service_role** key             |
| `REDIS_URL`            | no       | `redis://127.0.0.1:6379`       | Redis connection string (Redis Cloud)     |
| `PORT`                 | no       | `8080`                         | Server listen port                        |
| `RATE_LIMIT_REQUESTS`  | no       | `100`                          | Max requests per IP per window            |
| `RATE_LIMIT_WINDOW_SEC`| no       | `60`                           | Rate limit window in seconds              |
| `ALLOWED_ORIGINS`      | no       | `*`                            | Comma-separated CORS origins              |
| `LOG_FILE`             | no       | _(stdout)_                     | Log file path inside container            |
| `WORKER_THREADS`       | no       | `4`                            | Thread pool size (keep 4 on free tier)    |
| `FACE_SERVICE_URL`     | no       | -                              | Modal face service base URL               |
| `FACE_API_SECRET`      | no       | -                              | Shared secret for face service auth       |
| `FACE_THRESHOLD`       | no       | `0.82`                         | Face similarity threshold (0.0–1.0)       |

### Face Service (Modal secrets - `votestack-face-secrets`)

| Variable               | Required | Default         | Description                                |
|------------------------|----------|-----------------|--------------------------------------------|
| `FACE_API_SECRET`      | yes      | -               | Must match `FACE_API_SECRET` on backend    |
| `FACE_THRESHOLD`       | no       | `0.82`          | Cosine similarity threshold                |
| `INSIGHTFACE_MODEL`    | no       | `buffalo_sc`    | `buffalo_sc` (CPU) or `buffalo_l` (GPU)    |
| `FACE_ALLOWED_ORIGINS` | no       | Render URL      | CORS origins for face service              |

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

| Method | Endpoint                                         | Auth | Description                    |
|--------|--------------------------------------------------|------|--------------------------------|
| GET    | `/api/elections`                                 | yes  | List elections for user        |
| POST   | `/api/elections`                                 | yes  | Create election                |
| GET    | `/api/elections/:id`                             | yes  | Get election                   |
| DELETE | `/api/elections/:id`                             | yes  | Delete election and all data   |
| PATCH  | `/api/elections/:id/schedule`                    | yes  | Update schedule settings       |
| PATCH  | `/api/elections/:id/face-verify`                 | yes  | Toggle face verification on/off|
| GET    | `/api/elections/:id/candidates`                  | yes  | List candidates                |
| POST   | `/api/elections/:id/candidates`                  | yes  | Add candidate                  |
| DELETE | `/api/elections/:id/candidates`                  | yes  | Remove candidate               |
| GET    | `/api/elections/:id/voters`                      | yes  | List registered voters         |
| POST   | `/api/elections/:id/voters`                      | yes  | Register voter                 |
| DELETE | `/api/elections/:id/voters`                      | yes  | Remove voter                   |
| POST   | `/api/elections/:id/voters/sync`                 | yes  | Batch-replace voter list       |
| GET    | `/api/elections/:id/voters/:vid/enroll-face`     | yes  | Check if voter face enrolled   |
| POST   | `/api/elections/:id/voters/:vid/enroll-face`     | yes  | Enroll voter face (1–3 photos) |
| GET    | `/api/elections/:id/positions`                   | yes  | List positions (multi-ballot)  |
| POST   | `/api/elections/:id/positions`                   | yes  | Add position                   |
| DELETE | `/api/elections/:id/positions/:posId`            | yes  | Remove position                |

### Public Voting (no auth)

| Method | Endpoint                        | Description                              |
|--------|---------------------------------|------------------------------------------|
| GET    | `/api/vote/:id/candidates`      | Candidate list for ballot                |
| GET    | `/api/vote/:id/info`            | Election info incl. `face_verify_enabled`|
| POST   | `/api/vote/:id/check`           | Verify voter ID, check if voted          |
| POST   | `/api/vote/:id/verify-face`     | Verify voter face (fetch emb from DB)    |
| POST   | `/api/vote/:id/cast`            | Cast vote (atomic, duplicate-safe)       |
| GET    | `/api/vote/:id/results`         | Live results                             |
| GET    | `/api/multi-vote/:id/positions` | Full ballot for multi-position           |
| GET    | `/api/multi-vote/:id/info`      | Multi election info + face_verify flag   |
| POST   | `/api/multi-vote/:id/check`     | Check voter for multi-position           |
| POST   | `/api/multi-vote/:id/cast`      | Cast all position votes atomically       |
| GET    | `/api/multi-vote/:id/results`   | Multi-position live results              |

### Face Service (Modal - called by C++ backend only)

| Method | Endpoint                   | Auth   | Description                          |
|--------|----------------------------|--------|--------------------------------------|
| GET    | `/health`                  | none   | Health check + current threshold     |
| POST   | `/generate-embedding`      | secret | Photos → face embeddings (enroll)    |
| POST   | `/verify`                  | secret | Live frame + stored embeddings → result |

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

| Concern                   | Implementation                                                          |
|---------------------------|-------------------------------------------------------------------------|
| Password hashing          | PBKDF2-SHA256, 100k iterations, random salt via `RAND_bytes`            |
| Password comparison       | Constant-time via `CRYPTO_memcmp`                                       |
| Session tokens            | 32 random bytes via `RAND_bytes` (64-char hex), Redis + Supabase        |
| Duplicate vote prevention | `INSERT ON CONFLICT DO NOTHING` in single DB transaction                |
| Rate limiting             | Redis INCR + EXPIRE per IP, 100 req / 60s default                      |
| CORS                      | Origin validated against `ALLOWED_ORIGINS` allowlist                    |
| Auth guards               | Every protected route validates token before any DB access              |
| Data isolation            | Every query scoped to authenticated `user_id`                           |
| XSS prevention            | `escHtml()` sanitiser on all user-supplied output                       |
| Request size cap          | 1 MB body limit, 5s receive timeout per connection                      |
| TLS                       | nginx terminates HTTPS, TLSv1.2/1.3 only, HSTS enabled                 |
| Face biometrics           | Embeddings stored encrypted; raw photos never persisted (Change 6)      |
| Face liveness             | MediaPipe blink/head-movement check prevents static photo spoofing      |
| Face service auth         | Shared secret in `Authorization: Bearer` header between C++ and Modal   |
| Stateless face service    | Python service never accesses DB - C++ owns all data (Change 1)         |
| Configurable threshold    | `FACE_THRESHOLD` env var - no code change needed to tune accuracy        |

---

## Monitoring

Prometheus scrapes the Render backend every 10 seconds via `GET /metrics` (Prometheus text format).

**Run locally** (Docker Desktop required):

```bash
docker compose -f monitoring/docker-compose.monitoring.yml up -d
```

| Service    | URL                    |
|------------|------------------------|
| Grafana    | http://localhost:3001  |
| Prometheus | http://localhost:9090  |

The pre-loaded **VoteStack Overview** dashboard shows:

- Requests/sec and total request count
- Latency histogram (p50 / p95 / p99)
- 5xx error rate
- Active connections
- HTTP status breakdown (2xx / 4xx / 5xx)

Alert rules are defined in `monitoring/alert_rules.yml` and fire on:

- Any backend instance unreachable for more than 1 minute
- 5xx error rate above 5% for 2 minutes
- p99 latency above 2 seconds for 3 minutes
- Redis unreachable or above 80% memory

---

## Load Testing

Requires [k6](https://k6.io/docs/get-started/installation/).

```bash
# Install k6 on Windows
winget install k6 --source winget

# Smoke test - 5 VUs, 20s (run before every deploy)
k6 run load-test/smoke_test.js

# Full staged load test (ramps 10 → 50 → 100 VUs)
k6 run --env ELECTION_ID=<your-election-uuid> load-test/load_test.js

# Save results to JSON
k6 run --out json=load-test/results.json load-test/load_test.js
```

Default thresholds: p95 latency < 3s (Render free tier), error rate < 5%.

See `load-test/README.md` for full instructions and benchmark interpretation.

---

## Developer

**Naren S J**  
narensonu1520@gmail.com

[Project Documentation](https://deepwiki.com/Naren1520/Voting_Management_system)

## License

VoteStack is proprietary software.

The source code is not open source and may not be copied, modified,
redistributed, or used without explicit written permission.

Users are permitted to access and use the official deployed VoteStack
application only.