// ============================================================
// load_test.js - VoteStack staged load test
//
// Tests the full request lifecycle under increasing concurrency:
//   - Anonymous public voting (60% of traffic)
//   - Authenticated flows: signup → login → list elections (35%)
//   - Health checks (5%)
//
// Usage:
//   # Full staged test
//   k6 run --env BASE_URL=https://votestack-cjom.onrender.com load-test/load_test.js
//
//   # Quick 30s test at 10 VUs
//   k6 run --env BASE_URL=https://votestack-cjom.onrender.com --vus 10 --duration 30s load-test/load_test.js
//
//   # With a real election ID for vote testing
//   k6 run --env BASE_URL=https://votestack-cjom.onrender.com --env ELECTION_ID=<uuid> load-test/load_test.js
// ============================================================

import http from 'k6/http';
import { check, group, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';

// ── Custom metrics ────────────────────────────────────────────────────────────
const errorRate        = new Rate('error_rate');
const loginLatency     = new Trend('login_latency',     true);
const electionsLatency = new Trend('elections_latency', true);
const voteLatency      = new Trend('vote_latency',      true);
const rateLimited      = new Counter('rate_limited_429');

// ── Configuration ─────────────────────────────────────────────────────────────
const BASE_URL    = __ENV.BASE_URL    || 'https://votestack-cjom.onrender.com';
const ELECTION_ID = __ENV.ELECTION_ID || '00000000-0000-0000-0000-000000000000';

// Pre-registered voter IDs - replace with real IDs from your election
const VOTER_IDS = ['V001', 'V002', 'V003', 'V004', 'V005',
                   'V006', 'V007', 'V008', 'V009', 'V010'];

// ── Test stages ───────────────────────────────────────────────────────────────
export const options = {
  stages: [
    { duration: '30s', target: 10  },   // warm-up (Render free cold start)
    { duration: '60s', target: 50  },   // steady load - free tier limit
    { duration: '60s', target: 100 },   // push it
    { duration: '30s', target: 0   },   // ramp down
  ],
  thresholds: {
    http_req_duration:  ['p(95)<3000'],  // p95 under 3s (Render free tier)
    error_rate:         ['rate<0.05'],   // <5% errors acceptable on free tier
    login_latency:      ['p(99)<5000'],  // login includes PBKDF2 (CPU heavy)
    elections_latency:  ['p(99)<3000'],
  },
};

// ── Helpers ───────────────────────────────────────────────────────────────────
const JSON_HEADERS = { 'Content-Type': 'application/json' };

function pick(arr) {
  return arr[Math.floor(Math.random() * arr.length)];
}

function authHeaders(token) {
  return { ...JSON_HEADERS, 'Authorization': `Bearer ${token}` };
}

// ── Scenario 1: Public vote check (no auth required) ─────────────────────────
function publicVoteScenario() {
  // Get election info
  const infoRes = http.get(`${BASE_URL}/api/vote/${ELECTION_ID}/info`);
  check(infoRes, {
    'vote/info: no 5xx': (r) => r.status < 500,
  });
  if (infoRes.status === 429) { rateLimited.add(1); return; }

  // Get candidates
  const candRes = http.get(`${BASE_URL}/api/vote/${ELECTION_ID}/candidates`);
  check(candRes, {
    'vote/candidates: no 5xx': (r) => r.status < 500,
  });
  voteLatency.add(candRes.timings.duration);
  errorRate.add(candRes.status >= 500);
  if (candRes.status === 429) { rateLimited.add(1); return; }

  // Check voter ID
  const voterId = pick(VOTER_IDS);
  const checkRes = http.post(
    `${BASE_URL}/api/vote/${ELECTION_ID}/check`,
    JSON.stringify({ voter_id: voterId }),
    { headers: JSON_HEADERS }
  );
  check(checkRes, {
    'vote/check: 200 or 400': (r) => r.status === 200 || r.status === 400,
  });
  errorRate.add(checkRes.status >= 500);
}

// ── Scenario 2: Auth flow ─────────────────────────────────────────────────────
function authScenario() {
  // Use unique email per VU to avoid "email already registered" errors
  const ts       = Date.now();
  const email    = `loadtest_vu${__VU}_${ts}@votestack-test.com`;
  const password = 'LoadTest@123';

  // Signup
  const signupRes = http.post(
    `${BASE_URL}/api/auth/signup`,
    JSON.stringify({ name: 'Load Tester', email, password }),
    { headers: JSON_HEADERS }
  );
  errorRate.add(signupRes.status >= 500);
  if (signupRes.status === 429) { rateLimited.add(1); return; }
  if (signupRes.status !== 201) return; // skip if signup failed

  // Login immediately after
  const loginRes = http.post(
    `${BASE_URL}/api/auth/login`,
    JSON.stringify({ email, password }),
    { headers: JSON_HEADERS }
  );
  loginLatency.add(loginRes.timings.duration);

  const loginOk = check(loginRes, {
    'login: status 200':    (r) => r.status === 200,
    'login: returns token': (r) => {
      try { return !!JSON.parse(r.body).token; } catch { return false; }
    },
  });
  errorRate.add(!loginOk);
  if (!loginOk) return;

  let token;
  try { token = JSON.parse(loginRes.body).token; } catch { return; }

  // List elections (authenticated)
  const electionsRes = http.get(
    `${BASE_URL}/api/elections`,
    { headers: authHeaders(token) }
  );
  electionsLatency.add(electionsRes.timings.duration);
  check(electionsRes, {
    'elections: status 200': (r) => r.status === 200,
  });
  errorRate.add(electionsRes.status >= 500);
  if (electionsRes.status === 429) rateLimited.add(1);

  // Token ping (session validation via Redis)
  const pingRes = http.get(
    `${BASE_URL}/api/auth/ping`,
    { headers: authHeaders(token) }
  );
  check(pingRes, {
    'ping: status 200': (r) => r.status === 200,
  });
}

// ── Scenario 3: Health check ──────────────────────────────────────────────────
function healthScenario() {
  const res = http.get(`${BASE_URL}/health`);
  check(res, { 'health: 200': (r) => r.status === 200 });
  errorRate.add(res.status !== 200);
}

// ── Main VU loop ──────────────────────────────────────────────────────────────
export default function () {
  const roll = Math.random();

  if (roll < 0.60) {
    group('public_vote', publicVoteScenario);
  } else if (roll < 0.95) {
    group('auth_flow', authScenario);
  } else {
    group('health', healthScenario);
  }

  // Think time: 200ms–1s (simulates real user pacing)
  sleep(0.2 + Math.random() * 0.8);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
export function setup() {
  console.log(`\n========================================`);
  console.log(`VoteStack Load Test`);
  console.log(`Target:     ${BASE_URL}`);
  console.log(`ElectionID: ${ELECTION_ID}`);
  console.log(`========================================\n`);

  // Verify server is up before starting
  const health = http.get(`${BASE_URL}/health`);
  if (health.status !== 200) {
    console.error('Server is not healthy! Aborting.');
  }
  return {};
}

export function teardown() {
  console.log('\nLoad test complete. Check results above.');
}
