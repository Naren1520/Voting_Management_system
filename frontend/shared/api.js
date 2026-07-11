/**
 * VoteStack API Client
 * Centralised fetch wrapper for all backend calls
 */

const API = (() => {
  const base = () => window.API_BASE || 'http://localhost:8080';

  // Auth token now lives in an HttpOnly cookie set by the server.
  // We never read or write it from JS — the browser sends it automatically
  // on every same-origin / credentialed request.

  const headers = () => ({ 'Content-Type': 'application/json' });

  const req = async (method, path, body = null) => {
    const opts = {
      method,
      headers: headers(),
      // Required: tells the browser to send the HttpOnly session cookie
      // on cross-origin requests to the API server.
      credentials: 'include',
    };
    if (body) opts.body = JSON.stringify(body);

    // 35-second timeout - enough for Render free tier cold start (~30s).
    // AbortController lets us cancel the fetch after the timeout.
    const controller = new AbortController();
    const timeoutId  = setTimeout(() => controller.abort(), 35000);
    opts.signal = controller.signal;

    try {
      const res = await fetch(base() + path, opts);
      clearTimeout(timeoutId);
      const data = await res.json();
      return data;
    } catch (e) {
      clearTimeout(timeoutId);
      // AbortError = our timeout fired = server still waking up
      if (e.name === 'AbortError') {
        return { success: false, message: 'Server is waking up - please try again in a moment.' };
      }
      return { success: false, message: 'Network error - is the server running?' };
    }
  };

  // reqWithTimeout - like req() but with a custom timeout in milliseconds.
  // Used for face verification: Modal cold starts can take 30-60s, and the
  // C++ backend waits up to 90s for the Python service — 35s is not enough.
  const reqWithTimeout = async (timeoutMs, method, path, body = null) => {
    const opts = {
      method,
      headers: headers(),
      credentials: 'include',
    };
    if (body) opts.body = JSON.stringify(body);

    const controller = new AbortController();
    const timeoutId  = setTimeout(() => controller.abort(), timeoutMs);
    opts.signal = controller.signal;

    try {
      const res = await fetch(base() + path, opts);
      clearTimeout(timeoutId);
      const data = await res.json();
      return data;
    } catch (e) {
      clearTimeout(timeoutId);
      if (e.name === 'AbortError') {
        return { success: false, message: 'Face service is waking up — please try again in a moment.' };
      }
      return { success: false, message: 'Network error - is the server running?' };
    }
  };

  return {
    // Auth
    signup:  (name, email, password) =>
      req('POST', '/api/auth/signup', { name, email, password }),
    login:   (email, password) =>
      req('POST', '/api/auth/login', { email, password }),
    logout:  () => req('POST', '/api/auth/logout'),
    changePassword: (currentPassword, newPassword) =>
      req('POST', '/api/auth/change-password', { current_password: currentPassword, new_password: newPassword }),

    // Sessions
    getSessions:          ()          => req('GET',    '/api/auth/sessions'),
    revokeSession:        (sessionId) => req('DELETE', `/api/auth/sessions/${sessionId}`),
    revokeAllSessions:    ()          => req('DELETE', '/api/auth/sessions'),
    ping:                 ()          => req('GET',    '/api/auth/ping'),

    // Elections
    getElections:    ()                          => req('GET',    '/api/elections'),
    createElection:  (title, type='standard', sched={}) =>
      req('POST', '/api/elections', { title, election_type: type, ...sched }),
    getElection:     (id)                        => req('GET',    `/api/elections/${id}`),
    deleteElection:  (id)                        => req('DELETE', `/api/elections/${id}`),
    updateSchedule:  (id, sched)                 => req('PATCH',  `/api/elections/${id}/schedule`, sched),
    toggleFaceVerify:(id, enabled)               => req('PATCH',  `/api/elections/${id}/face-verify`, { face_verify_enabled: enabled }),

    // Positions (multi elections)
    getPositions:    (elecId)             => req('GET',    `/api/elections/${elecId}/positions`),
    addPosition:     (elecId, title)      => req('POST',   `/api/elections/${elecId}/positions`, { title }),
    deletePosition:  (elecId, posId)      => req('DELETE', `/api/elections/${elecId}/positions/${posId}`),
    getPosCandidates:(elecId, posId)      => req('GET',    `/api/elections/${elecId}/positions/${posId}/candidates`),
    addPosCandidate: (elecId, posId, name)=> req('POST',   `/api/elections/${elecId}/positions/${posId}/candidates`, { name }),
    delPosCandidate: (elecId, posId, name)=> req('DELETE', `/api/elections/${elecId}/positions/${posId}/candidates`, { name }),

    // Public multi-vote (no auth)
    getMultiBallot:  (elecId)             => req('GET',  `/api/multi-vote/${elecId}/positions`),
    getMultiInfo:    (elecId)             => req('GET',  `/api/multi-vote/${elecId}/info`),
    checkMultiVoter: (elecId, voter_id)   => req('POST', `/api/multi-vote/${elecId}/check`,     { voter_id }),
    castMultiVotes:  (elecId, voter_id, votes) => req('POST', `/api/multi-vote/${elecId}/cast`, { voter_id, votes }),
    getMultiResults: (elecId)             => req('GET',  `/api/multi-vote/${elecId}/results`),

    // Candidates
    getCandidates:   (elecId)      => req('GET',    `/api/elections/${elecId}/candidates`),
    addCandidate:    (elecId, name)=> req('POST',   `/api/elections/${elecId}/candidates`, { name }),
    deleteCandidate: (elecId, name)=> req('DELETE', `/api/elections/${elecId}/candidates`, { name }),

    // Voters (admin)
    getVoters:    (elecId)         => req('GET',    `/api/elections/${elecId}/voters`),
    addVoter:     (elecId, v)      => req('POST',   `/api/elections/${elecId}/voters`, v),
    syncVoters:   (elecId, voters) => req('POST',   `/api/elections/${elecId}/voters/sync`, { voters }),
    deleteVoter:  (elecId, voter_id) =>
      req('DELETE', `/api/elections/${elecId}/voters`, { voter_id }),
    getVotedIds:  (elecId)         => req('GET',    `/api/elections/${elecId}/voted`),

    // Face verification
    enrollFace: (elecId, voterId, photos) =>
      req('POST', `/api/elections/${elecId}/voters/${encodeURIComponent(voterId)}/enroll-face`, { photos }),
    checkFaceEnrolled: (elecId, voterId) =>
      req('GET', `/api/elections/${elecId}/voters/${encodeURIComponent(voterId)}/enroll-face`),
    // Face verify uses a 120s timeout — Modal cold starts can take 30-60s,
    // and the C++ backend waits up to 90s for the Python service to respond.
    verifyFace: (elecId, voterId, frames, threshold) =>
      reqWithTimeout(120000, 'POST', `/api/vote/${elecId}/verify-face`, { voter_id: voterId, frames, ...(threshold ? { threshold } : {}) }),
    publicCandidates: (elecId) =>
      req('GET',  `/api/vote/${elecId}/candidates`),
    getElectionInfo: (elecId) =>
      req('GET',  `/api/vote/${elecId}/info`),
    checkVoter: (elecId, voter_id) =>
      req('POST', `/api/vote/${elecId}/check`, { voter_id }),
    castVote:   (elecId, voter_id, candidate_name) =>
      req('POST', `/api/vote/${elecId}/cast`, { voter_id, candidate_name }),
    getResults: (elecId) =>
      req('GET',  `/api/vote/${elecId}/results`),
  };
})();

// Auth helpers
//
// The session token is now stored exclusively in an HttpOnly cookie managed
// by the server. JS never touches it, which eliminates XSS-based token theft.
//
// We still keep a small, non-sensitive user object (id, name, email) in
// localStorage so the dashboard can greet the user and show their profile
// without an extra round-trip. Losing this data on XSS is harmless — it
// contains no credentials and no session material.
const Auth = {
  save(user) {
    // token is intentionally NOT stored here — it lives in the HttpOnly cookie.
    localStorage.setItem('vs_user', JSON.stringify(user));
  },
  clear() {
    localStorage.removeItem('vs_user');
  },
  // isLoggedIn checks for the presence of the cached user object.
  // The cookie itself is validated server-side on every API call.
  isLoggedIn() { return !!localStorage.getItem('vs_user'); },
  user() {
    try { return JSON.parse(localStorage.getItem('vs_user')); } catch { return null; }
  },
  requireAuth() {
    if (!this.isLoggedIn()) {
      window.location.href = '/auth/login.html';
      return false;
    }
    SessionGuard.start();
    return true;
  },
  requireGuest() {
    if (this.isLoggedIn()) {
      window.location.href = '/dashboard/index.html';
      return false;
    }
    return true;
  }
};

/**
 * SessionGuard
 * Polls /api/auth/ping every 30 seconds.
 * If the session has been revoked remotely, shows a full-screen
 * "You have been signed out" overlay instead of silently redirecting.
 *
 * Usage: call SessionGuard.start() once on any protected page.
 * It is started automatically by Auth.requireAuth() if the user is logged in.
 */
const SessionGuard = (() => {
  const POLL_MS = 30_000; // 30 seconds
  let timer     = null;
  let shown     = false;

  function injectOverlay() {
    if (document.getElementById('sg-overlay')) return;

    // Styles
    const style = document.createElement('style');
    style.textContent = `
      #sg-overlay {
        position: fixed; inset: 0; z-index: 99999;
        background: rgba(10,10,10,.72);
        backdrop-filter: blur(8px);
        -webkit-backdrop-filter: blur(8px);
        display: flex; align-items: center; justify-content: center;
        padding: 24px;
        animation: sg-fade-in .35s cubic-bezier(.22,.61,.36,1);
      }
      @keyframes sg-fade-in { from { opacity:0; } to { opacity:1; } }
      #sg-box {
        background: #fff;
        border-radius: 20px;
        padding: 40px 36px;
        max-width: 400px; width: 100%;
        text-align: center;
        box-shadow: 0 24px 64px rgba(0,0,0,.22);
        animation: sg-slide-up .35s cubic-bezier(.22,.61,.36,1);
      }
      @keyframes sg-slide-up {
        from { opacity:0; transform:translateY(20px); }
        to   { opacity:1; transform:translateY(0); }
      }
      #sg-icon {
        width: 60px; height: 60px; margin: 0 auto 20px;
        background: #fef2f2;
        border-radius: 50%;
        display: flex; align-items: center; justify-content: center;
      }
      #sg-icon svg { width: 28px; height: 28px; stroke: #dc2626; fill: none; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
      #sg-title {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', sans-serif;
        font-size: 1.25rem; font-weight: 800;
        color: #1a1a1a; letter-spacing: -.02em;
        margin-bottom: 10px;
      }
      #sg-desc {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', sans-serif;
        font-size: .9rem; color: #6e6e6e;
        line-height: 1.55; margin-bottom: 28px;
      }
      #sg-btn {
        display: inline-flex; align-items: center; justify-content: center; gap: 8px;
        width: 100%; padding: 13px 24px;
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', sans-serif;
        font-size: 14.5px; font-weight: 700;
        color: #fff; background: #1a1a1a;
        border: none; border-radius: 10px; cursor: pointer;
        transition: background .2s, transform .15s;
      }
      #sg-btn:hover { background: #2e2e2e; transform: translateY(-1px); }
      #sg-btn svg { width: 15px; height: 15px; stroke: currentColor; fill: none; stroke-width: 2.5; stroke-linecap: round; stroke-linejoin: round; }
    `;
    document.head.appendChild(style);

    // Markup
    const overlay = document.createElement('div');
    overlay.id = 'sg-overlay';
    overlay.innerHTML = `
      <div id="sg-box">
        <div id="sg-icon">
          <svg viewBox="0 0 24 24">
            <rect x="3" y="11" width="18" height="11" rx="2" ry="2"/>
            <path d="M7 11V7a5 5 0 0 1 10 0v4"/>
          </svg>
        </div>
        <div id="sg-title">You've been signed out</div>
        <div id="sg-desc">
          This session was signed out from another device.<br/>
          Please sign in again to continue.
        </div>
        <button id="sg-btn" onclick="SessionGuard.goLogin()">
          <svg viewBox="0 0 24 24"><path d="M15 3h4a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-4"/><polyline points="10 17 15 12 10 7"/><line x1="15" y1="12" x2="3" y2="12"/></svg>
          Sign in again
        </button>
      </div>`;
    document.body.appendChild(overlay);
  }

  async function check() {
    if (!Auth.isLoggedIn()) return;
    try {
      const res = await API.ping();
      if (!res.success) showSignedOut();
    } catch (_) {
      // network error - don't show overlay, try again next tick
    }
  }

  function showSignedOut() {
    if (shown) return;
    shown = true;
    stop();
    Auth.clear();
    injectOverlay();
  }

  function start() {
    if (timer) return; // already running
    // First check after 30s, then every 30s
    timer = setInterval(check, POLL_MS);
  }

  function stop() {
    if (timer) { clearInterval(timer); timer = null; }
  }

  function goLogin() {
    window.location.href = '/auth/login.html';
  }

  return { start, stop, goLogin };
})();
