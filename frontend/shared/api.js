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
