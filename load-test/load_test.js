// ============================================================
// load_test.js — Step 13: k6 load test for VoteStack
//
// Stages:
//   0→500 VUs  over  30s  — ramp up
//   500 VUs    for   60s  — sustained load
//   500→2000   over  60s  — stress test
//   2000 VUs   for   60s  — peak hold
//   2000→0     over  30s  — ramp down
//
// Run:
//   k6 run --out json=results.json load_test.js
//   k6 run --vus 100 --duration 30s load_test.js   (quick smoke test)
// ============================================================

import http from 'k6/http';
import { check, group, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';

// ── Custom metrics ────────────────────────────────────────────────────────────
const errorRate        = new Rate('error_rate');
const loginLatency     = new Trend('login_latency',    true);
const electionsLatency = new Trend('elections_latency', true);
const voteLatency      = new Trend('vote_latency',      true);
const rateLimited      = new Counter('rate_limited_429');

// ── Configuration ─────────────────────────────────────────────────────────────
const BASE_URL = __ENV.BASE_URL || 'https://your-domain.com';

// A list of pre-seeded election IDs to use in public vote tests.
// Replace with real election IDs from your Supabase DB.
const ELECTION_IDS = [
  __ENV.ELECTION_ID_1 || 'election-uuid-1',
  __ENV.ELECTION_ID_2 || 'election-uuid-2',
];

// Pre-registered voter IDs (must exist in the voters table)
const VOTER_IDS = ['V001', 'V002', 'V003', 'V004', 'V005'];

// ── Test stages ───────────────────────────────────────────────────────────────
export const options = {
  stages: [
    { duration: '30s',  target: 100  },  // warm-up
    { duration: '60s',  target: 500  },  // steady load
    { duration: '60s',  target: 2000 },  // stress ramp
    { duration: '60s',  target: 2000 },  // peak hold
    { duration: '30s',  target: 0    },  // ramp down
  ],
  thresholds: {
    // 95% of all requests must complete under 500ms
    http_req_duration: ['p(95)<500'],
    // Less than 1% errors
    error_rate: ['rate<0.01'],
    // p99 login under 1s
    login_latency: ['p(99)<1000'],
    // p99 elections list under 800ms
    elections_latency: ['p(99)<800'],
  },
};

// ── Shared headers ────────────────────────────────────────────────────────────
const jsonHeaders = { 'Content-Type': 'application/json' };

// ── Helper: pick random element ───────────────────────────────────────────────
function pick(arr) {
  return arr[Math.floor(Math.random() * arr.length)];
}

// ── Scenario: Anonymous public vote check ─────────────────────────────────────
function publicVoteCheck() {
  const electionId = pick(ELECTION_IDS);
  const voterId    = pick(VOTER_IDS);

  // Get candidate list
  const candidatesRes = http.get(`${BASE_URL}/api/vote/${electionId}/candidates`);
  check(candidatesRes, {
    'GET /vote/candidates status 200': (r) => r.status === 200,
  });
  errorRate.add(candidatesRes.status >= 400);
  if (candidatesRes.status === 429) rateLimited.add(1);

  // Check voter
  const checkRes = http.post(
    `${BASE_URL}/api/vote/${electionId}/check`,
    JSON.stringify({ voter_id: voterId }),
    { headers: jsonHeaders }
  );
  check(checkRes, {
    'POST /vote/check status 200': (r) => r.status === 200 || r.status === 400,
  });
  voteLatency.add(checkRes.timings.duration);
  errorRate.add(checkRes.status >= 500);
}

// ── Scenario: Auth flow ───────────────────────────────────────────────────────
function authFlow() {
  const email    = `loadtest_${__VU}_${Date.now()}@example.com`;
  const password = 'Test@12345678';

  // Signup (new user each VU iteration — stress the auth path)
  const signupRes = http.post(
    `${BASE_URL}/api/auth/signup`,
    JSON.stringify({ name: 'Load Tester', email, password }),
    { headers: jsonHeaders }
  );
  errorRate.add(signupRes.status >= 500);
  if (signupRes.status === 429) { rateLimited.add(1); return; }

  // Login with same credentials
  const loginStart = Date.now();
  const loginRes = http.post(
    `${BASE_URL}/api/auth/login`,
    JSON.stringify({ email, password }),
    { headers: jsonHeaders }
  );
  loginLatency.add(loginRes.timings.duration);

  const loginOk = check(loginRes, {
    'POST /auth/login status 200': (r) => r.status === 200,
    'login returns token': (r) => {
      try { return !!JSON.parse(r.body).token; } catch { return false; }
    },
  });
  errorRate.add(!loginOk);
  if (!loginOk || loginRes.status === 429) return;

  let token;
  try { token = JSON.parse(loginRes.body).token; } catch { return; }
  const authHeaders = { ...jsonHeaders, 'Authorization': `Bearer ${token}` };

  // List elections (authenticated)
  const electionsRes = http.get(`${BASE_URL}/api/elections`, { headers: authHeaders });
  electionsLatency.add(electionsRes.timings.duration);
  check(electionsRes, {
    'GET /elections status 200': (r) => r.status === 200,
  });
  errorRate.add(electionsRes.status >= 500);
  if (electionsRes.status === 429) rateLimited.add(1);
}

// ── Scenario: Health check (baseline) ────────────────────────────────────────
function healthCheck() {
  const res = http.get(`${BASE_URL}/health`);
  check(res, { '/health returns 200': (r) => r.status === 200 });
  errorRate.add(res.status !== 200);
}

// ── Main virtual user loop ────────────────────────────────────────────────────
export default function () {
  const roll = Math.random();

  if (roll < 0.60) {
    // 60% — anonymous public vote checks (most traffic in production)
    group('public_vote', publicVoteCheck);
  } else if (roll < 0.95) {
    // 35% — authenticated flows (signup/login/list)
    group('auth_flow', authFlow);
  } else {
    // 5% — health checks
    group('health', healthCheck);
  }

  // Think time: 0–500ms random pause to simulate real users
  sleep(Math.random() * 0.5);
}

// ── Setup: print test configuration ──────────────────────────────────────────
export function setup() {
  console.log(`Target: ${BASE_URL}`);
  console.log(`Elections: ${ELECTION_IDS.join(', ')}`);
  return {};
}

// ── Teardown: print summary ────────────────────────────────────────────────────
export function teardown(data) {
  console.log('Load test complete.');
}
