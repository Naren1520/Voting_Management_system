/**
 * VoteStack API Client
 * Centralised fetch wrapper for all backend calls
 */

const API = (() => {
  const base = () => window.API_BASE || 'http://localhost:8080';

  const token = () => localStorage.getItem('vs_token') || '';

  const headers = (auth = true) => {
    const h = { 'Content-Type': 'application/json' };
    if (auth && token()) h['Authorization'] = 'Bearer ' + token();
    return h;
  };

  const req = async (method, path, body = null, auth = true) => {
    const opts = { method, headers: headers(auth) };
    if (body) opts.body = JSON.stringify(body);
    try {
      const res = await fetch(base() + path, opts);
      const data = await res.json();
      return data;
    } catch (e) {
      return { success: false, message: 'Network error — is the server running?' };
    }
  };

  return {
    // Auth
    signup:  (name, email, password) =>
      req('POST', '/api/auth/signup', { name, email, password }, false),
    login:   (email, password) =>
      req('POST', '/api/auth/login', { email, password }, false),
    logout:  () => req('POST', '/api/auth/logout'),
    changePassword: (currentPassword, newPassword) =>
      req('POST', '/api/auth/change-password', { current_password: currentPassword, new_password: newPassword }),

    // Sessions
    getSessions:          ()          => req('GET',    '/api/auth/sessions'),
    revokeSession:        (sessionId) => req('DELETE', `/api/auth/sessions/${sessionId}`),
    revokeAllSessions:    ()          => req('DELETE', '/api/auth/sessions'),
    ping:                 ()          => req('GET',    '/api/auth/ping'),

    // Elections
    getElections:    ()            => req('GET',    '/api/elections'),
    createElection:  (title)       => req('POST',   '/api/elections', { title }),
    getElection:     (id)          => req('GET',    `/api/elections/${id}`),
    deleteElection:  (id)          => req('DELETE', `/api/elections/${id}`),

    // Candidates
    getCandidates:   (elecId)      => req('GET',    `/api/elections/${elecId}/candidates`),
    addCandidate:    (elecId, name)=> req('POST',   `/api/elections/${elecId}/candidates`, { name }),
    // DELETE with body — use POST with _method override handled server-side
    deleteCandidate: (elecId, name)=> req('DELETE', `/api/elections/${elecId}/candidates`, { name }),

    // Voters (admin)
    getVoters:    (elecId)         => req('GET',    `/api/elections/${elecId}/voters`),
    addVoter:     (elecId, v)      => req('POST',   `/api/elections/${elecId}/voters`, v),
    syncVoters:   (elecId, voters) => req('POST',   `/api/elections/${elecId}/voters/sync`, { voters }),
    deleteVoter:  (elecId, voter_id) =>
      req('DELETE', `/api/elections/${elecId}/voters`, { voter_id }),
    getVotedIds:  (elecId)         => req('GET',    `/api/elections/${elecId}/voted`),

    // Public voting (no auth)
    publicCandidates: (elecId) =>
      req('GET',  `/api/vote/${elecId}/candidates`, null, false),
    getElectionInfo: (elecId) =>
      req('GET',  `/api/vote/${elecId}/info`, null, false),
    checkVoter: (elecId, voter_id) =>
      req('POST', `/api/vote/${elecId}/check`, { voter_id }, false),
    castVote:   (elecId, voter_id, candidate_name) =>
      req('POST', `/api/vote/${elecId}/cast`, { voter_id, candidate_name }, false),
    getResults: (elecId) =>
      req('GET',  `/api/vote/${elecId}/results`, null, false),
  };
})();

// Auth helpers
const Auth = {
  save(token, user) {
    localStorage.setItem('vs_token', token);
    localStorage.setItem('vs_user', JSON.stringify(user));
  },
  clear() {
    localStorage.removeItem('vs_token');
    localStorage.removeItem('vs_user');
  },
  isLoggedIn() { return !!localStorage.getItem('vs_token'); },
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
      // network error — don't show overlay, try again next tick
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
