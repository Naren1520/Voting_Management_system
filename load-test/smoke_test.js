// ============================================================
// smoke_test.js — quick sanity check before a full load test
// Run: k6 run smoke_test.js
// ============================================================

import http   from 'k6/http';
import { check, sleep } from 'k6';

const BASE_URL = __ENV.BASE_URL || 'https://your-domain.com';

export const options = {
  vus:      5,
  duration: '15s',
  thresholds: {
    http_req_failed:   ['rate<0.01'],
    http_req_duration: ['p(95)<1000'],
  },
};

export default function () {
  // Health
  const h = http.get(`${BASE_URL}/health`);
  check(h, { 'health 200': (r) => r.status === 200 });

  // Unauthenticated public endpoint
  const c = http.get(`${BASE_URL}/api/vote/invalid-id/candidates`);
  check(c, { 'public vote 200 or 404': (r) => r.status === 200 || r.status === 404 });

  sleep(1);
}
