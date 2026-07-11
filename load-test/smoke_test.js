// smoke_test.js - VoteStack quick sanity check
//
// Run before every deploy to confirm nothing is broken.
//
// Usage:
//   k6 run load-test/smoke_test.js
//   k6 run --env BASE_URL=https://votestack-cjom.onrender.com load-test/smoke_test.js

import http from 'k6/http';
import { check, sleep } from 'k6';

const BASE_URL = __ENV.BASE_URL || 'https://votestack-cjom.onrender.com';

export const options = {
  vus:      5,
  duration: '20s',
  thresholds: {
    http_req_duration: ['p(95)<2000'],  // p95 under 2s (Render free cold start)
    // Note: http_req_failed not used - 404s from invalid test IDs are expected
  },
};

export default function () {
  // 1. Health check
  const health = http.get(`${BASE_URL}/health`);
  check(health, {
    'health: status 200':       (r) => r.status === 200,
    'health: body has status':  (r) => r.json('status') === 'ok',
  });

  sleep(0.5);

  // 2. Metrics endpoint
  const metrics = http.get(`${BASE_URL}/metrics`);
  check(metrics, {
    'metrics: status 200': (r) => r.status === 200,
    'metrics: has content': (r) => r.body.includes('http_requests_total'),
  });

  sleep(0.5);

  // 3. Public vote endpoint (invalid ID → 404, not 500)
  const vote = http.get(`${BASE_URL}/api/vote/00000000-0000-0000-0000-000000000000/candidates`);
  check(vote, {
    'vote/candidates: no 5xx': (r) => r.status < 500,
  });

  sleep(0.5);

  // 4. Auth signup with invalid data → 400, not 500
  const signup = http.post(
    `${BASE_URL}/api/auth/signup`,
    JSON.stringify({ name: '', email: '', password: '' }),
    { headers: { 'Content-Type': 'application/json' } }
  );
  check(signup, {
    'signup validation: 400': (r) => r.status === 400,
    'signup validation: has message': (r) => {
      try { return !!JSON.parse(r.body).message; } catch { return false; }
    },
  });

  sleep(1);
}
