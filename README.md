# VoteStack — Secure Online Voting Platform

> A full-stack online voting system built with a high-performance **C++ backend** and a modern **Vanilla JS frontend**. Supports multi-election management, registered voter lists, real-time results, and full data isolation per user.

<br/>

![C++](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-ES6+-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black)
![Docker](https://img.shields.io/badge/Docker-2496ED?style=for-the-badge&logo=docker&logoColor=white)
![Render](https://img.shields.io/badge/Render-46E3B7?style=for-the-badge&logo=render&logoColor=black)
![Supabase](https://img.shields.io/badge/Supabase-3ECF8E?style=for-the-badge&logo=supabase&logoColor=white)

<br/>

---

## Table of Contents

1. [Overview](#overview)
2. [Tech Stack](#tech-stack)
3. [Features](#features)
4. [System Architecture](#system-architecture)
5. [Project Structure](#project-structure)
6. [Getting Started](#getting-started)
7. [Backend — Build & Run](#backend--build--run)
8. [Frontend — Setup & Run](#frontend--setup--run)
9. [API Reference](#api-reference)
10. [Deployment](#deployment)
11. [Security](#security)
12. [Developer](#developer)

---

## Overview

VoteStack is a production-ready voting platform that allows any authenticated user to:

- Create and manage multiple independent elections
- Register voters with unique voter IDs
- Share a unique voting link per election
- Prevent duplicate voting at the server level
- View live results with vote breakdowns

Each election is fully isolated — candidates, voters, and votes are never mixed across elections or users.

---

## Tech Stack

### Backend
| Technology | Purpose |
|---|---|
| C++ 17 | Core server language |
| Raw TCP / HTTP (custom) | HTTP request handling without frameworks |
| nlohmann/json | JSON serialization & deserialization |
| Supabase REST API | Cloud database (via `curl`) |
| OpenSSL | Password hashing & token generation |
| POSIX Threads / pthreads | Multi-threaded request handling |

### Frontend
| Technology | Purpose |
|---|---|
| HTML5 | Page structure |
| CSS3 (custom design system) | Styling, animations, layout |
| Vanilla JavaScript (ES6+) | Client logic, API calls, auth |
| Lucide Icons | UI icon set |
| Google Fonts (Inter) | Typography |
| Fetch API | HTTP communication with backend |
| localStorage | Client-side session management |

### Infrastructure
| Technology | Purpose |
|---|---|
| Docker | Backend containerisation |
| Render | Backend cloud deployment |
| Netlify | Frontend static hosting |
| GitHub | Source control |

---

## Features

### Voter Experience
- Single-page voting flow with step indicator
- Voter ID verification before ballot is shown
- Duplicate vote prevention (server-enforced)
- Vote confirmation screen with lock badge

### Election Management
- Create unlimited elections per account
- Add / remove candidates dynamically
- Register voters (voter ID, name, email, phone)
- Toggle election active / closed status
- Shareable per-election voting link
- Delete elections with full data cleanup

### Results & Analytics
- Live vote count per candidate
- Visual progress bars scaled to max votes
- Total vote counter
- Refresh on demand

### Auth & Security
- JWT-based authentication
- Bcrypt password hashing (via OpenSSL)
- Per-user data isolation
- Guest route protection (redirect if not logged in)
- Auth route protection (redirect if already logged in)

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        BROWSER (Client)                         │
│                                                                 │
│   landing/    auth/    dashboard/    election/    vote/         │
│   index.html  login    index.html    manage.html  index.html    │
│              signup                                             │
│                    │                                            │
│         shared/api.js  +  shared/styles.css                     │
│              config.js (API_BASE URL)                           │
└──────────────────────────┬──────────────────────────────────────┘
                           │  HTTPS  (REST/JSON)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│               C++ HTTP Server  (port 8080)                      │
│                                                                 │
│   POST /api/auth/signup        POST /api/auth/login             │
│   GET  /api/elections          POST /api/elections              │
│   GET  /api/elections/:id      DELETE /api/elections/:id        │
│   GET  /api/elections/:id/candidates                            │
│   POST /api/elections/:id/candidates                            │
│   GET  /api/elections/:id/voters                                │
│   POST /api/elections/:id/voters                                │
│   GET  /api/vote/:id/candidates   (public)                      │
│   POST /api/vote/:id/check        (public)                      │
│   POST /api/vote/:id/cast         (public)                      │
│   GET  /api/vote/:id/results      (public)                      │
└──────────────────────────┬──────────────────────────────────────┘
                           │  HTTPS  (Supabase REST)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Supabase (PostgreSQL)                         │
│                                                                 │
│   users          elections        candidates                    │
│   voters         votes            sessions                      │
└─────────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
VotingSystem/
|
+-- backend/
|   |
|   +-- data/                          # Runtime data (auto-generated, git-ignored)
|   |   +-- candidates.json
|   |   +-- voters.json
|   |   \-- registered_voters.json
|   |
|   +-- json.hpp                       # nlohmann/json single-header library
|   +-- voting_server.cpp              # Windows build — main HTTP server source
|   \-- voting_server_linux.cpp        # Linux build — used by Docker
|
+-- frontend/
|   |
|   +-- assets/
|   |   \-- Logo.png                   # Application logo
|   |
|   +-- auth/
|   |   +-- login.html                 # Login page
|   |   \-- signup.html                # Registration page
|   |
|   +-- dashboard/
|   |   \-- index.html                 # Authenticated user dashboard (election list)
|   |
|   +-- election/
|   |   \-- manage.html                # Election management (candidates, voters, results)
|   |
|   +-- landing/
|   |   \-- index.html                 # Public landing / marketing page
|   |
|   +-- shared/
|   |   +-- api.js                     # Centralised API client + Auth helpers
|   |   \-- styles.css                 # Shared design system (tokens, components)
|   |
|   +-- vote/
|   |   \-- index.html                 # Public voter-facing ballot page
|   |
|   +-- config.js                      # Runtime config (API_BASE URL)
|   \-- netlify.toml                   # Netlify deployment config
|
+-- Dockerfile                         # Multi-stage Docker build (gcc -> debian-slim)
+-- render.yaml                        # Render.com deployment config
+-- run.bat                            # Local dev launcher (Windows)
\-- README.md
```

---

## Getting Started

### Prerequisites

| Requirement | Version | Notes |
|---|---|---|
| GCC / G++ | 12+ | For Linux/Windows build |
| Docker | 20+ | For containerised build |
| Python | 3.x | To serve frontend locally |
| Git | any | Clone the repo |

---

## Backend — Build & Run

### Option A — Local Build (Windows)

```bash
cd backend
g++ -std=c++17 -O2 -o voting_server.exe voting_server.cpp -lws2_32 -pthread
voting_server.exe
```

### Option B — Local Build (Linux / macOS)

```bash
cd backend
g++ -std=c++17 -O2 -o voting_server voting_server_linux.cpp -pthread
./voting_server
```

### Option C — Docker Build

```bash
# Build the image
docker build -t votestack-backend .

# Run the container
docker run -p 8080:8080 votestack-backend
```

Server starts on **http://localhost:8080**

### Quick Start (Windows — One Command)

```bat
.\run.bat
```

This starts the backend on port `8080` and the frontend on port `3000`, then opens the browser automatically.

---

## Frontend — Setup & Run

### Option A — Python HTTP Server (Recommended for local dev)

```bash
cd frontend
python -m http.server 3000
```

Open **http://localhost:3000/landing/index.html**

### Option B — Node.js HTTP Server

```bash
cd frontend
npx http-server -p 3000
```

### Option C — VS Code Live Server

- Install the **Live Server** extension
- Right-click `frontend/landing/index.html` → **Open with Live Server**

### Configuring the API URL

Edit `frontend/config.js` to point at your backend:

```js
// Local development
window.API_BASE = 'http://localhost:8080';

// Production (Render)
window.API_BASE = 'https://voting-management-system-1-zh39.onrender.com';
```

---

## API Reference

### Base URL

```
https://voting-management-system-1-zh39.onrender.com
```

### Authentication

All protected routes require:

```
Authorization: Bearer <jwt_token>
```

---

### Auth

#### POST `/api/auth/signup`
Register a new user.

**Request body:**
```json
{
  "name": "John Doe",
  "email": "john@example.com",
  "password": "secret123"
}
```

**Response:**
```json
{
  "success": true,
  "token": "<jwt>",
  "user": { "id": "...", "name": "John Doe", "email": "john@example.com" }
}
```

---

#### POST `/api/auth/login`
Authenticate an existing user.

**Request body:**
```json
{
  "email": "john@example.com",
  "password": "secret123"
}
```

**Response:**
```json
{
  "success": true,
  "token": "<jwt>",
  "user": { "id": "...", "name": "John Doe", "email": "john@example.com" }
}
```

---

### Elections

#### GET `/api/elections`  *(auth required)*
Returns all elections owned by the authenticated user.

#### POST `/api/elections`  *(auth required)*
Create a new election.

**Request body:**
```json
{ "title": "Student Council 2025" }
```

#### GET `/api/elections/:id`  *(auth required)*
Get a single election by ID.

#### DELETE `/api/elections/:id`  *(auth required)*
Permanently delete an election and all its data.

---

### Candidates

#### GET `/api/elections/:id/candidates`  *(auth required)*
List all candidates for an election.

#### POST `/api/elections/:id/candidates`  *(auth required)*
Add a candidate.

```json
{ "name": "Alice Johnson" }
```

#### DELETE `/api/elections/:id/candidates`  *(auth required)*
Remove a candidate.

```json
{ "name": "Alice Johnson" }
```

---

### Voters

#### GET `/api/elections/:id/voters`  *(auth required)*
List all registered voters for an election.

#### POST `/api/elections/:id/voters`  *(auth required)*
Register a voter.

```json
{
  "voter_id": "V001",
  "name": "Jane Smith",
  "email": "jane@example.com",
  "phone": "+91..."
}
```

#### DELETE `/api/elections/:id/voters`  *(auth required)*
Remove a registered voter.

```json
{ "voter_id": "V001" }
```

---

### Public Voting  *(no auth required)*

#### GET `/api/vote/:id/candidates`
Get the candidate list for a public ballot page.

#### GET `/api/vote/:id/info`
Get the election title and active status.

#### POST `/api/vote/:id/check`
Verify a voter ID before showing the ballot.

```json
{ "voter_id": "V001" }
```

**Response:**
```json
{ "success": true, "already_voted": false }
```

#### POST `/api/vote/:id/cast`
Cast a vote.

```json
{
  "voter_id": "V001",
  "candidate_name": "Alice Johnson"
}
```

#### GET `/api/vote/:id/results`
Get live results for an election.

```json
{
  "success": true,
  "total_votes": 12,
  "candidates": [
    { "name": "Alice Johnson", "votes": 8 },
    { "name": "Bob Smith", "votes": 4 }
  ]
}
```

---

## Deployment

### Backend — Render.com

The backend is containerised with Docker and deployed to [Render](https://render.com) via `render.yaml`.

```yaml
services:
  - type: web
    name: voting-system-backend
    env: docker
    dockerfilePath: ./Dockerfile
    branch: main
    plan: free
    healthCheckPath: /candidates
```

Push to `main` branch triggers an automatic redeploy.

### Frontend — Netlify

The `frontend/` directory is deployed as a static site on Netlify.

```toml
# frontend/netlify.toml
```

Set the **Publish directory** to `frontend/` in Netlify dashboard settings.

**Environment variable to set in Netlify:**
```
API_BASE = https://voting-management-system-1-zh39.onrender.com
```

---

## Security

| Concern | Implementation |
|---|---|
| Password storage | Bcrypt hashing via OpenSSL |
| Session management | JWT tokens with expiry |
| Duplicate vote prevention | Server-side voter ID check per election |
| Data isolation | All queries scoped to authenticated user ID |
| XSS prevention | `escHtml()` sanitiser on all user-supplied output |
| CORS | Configured on backend for allowed origins |
| Auth guards | `Auth.requireAuth()` / `Auth.requireGuest()` on every page load |

---

## Developer

**Naren S J**
📧 [narensonu1520@gmail.com](mailto:narensonu1520@gmail.com)

---

*Built with a C++ backend for performance, Supabase for persistence, and a fully custom frontend design system — no UI frameworks used.*
