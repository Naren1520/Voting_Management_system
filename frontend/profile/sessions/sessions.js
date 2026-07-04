/**
 * sessions.js
 * Displays all active login sessions for the current VoteStack admin.
 * Each session shows: device (browser + OS), location, login time.
 * Other sessions can be revoked individually or all at once.
 */

document.addEventListener('DOMContentLoaded', () => {
  if (!Auth.requireAuth()) return;
  loadSessions();
});

/* ----------------------------------------------------------
   Load & render sessions
---------------------------------------------------------- */
async function loadSessions() {
  showLoading(true);
  const res = await API.getSessions();
  showLoading(false);

  if (!res.success) {
    showToast('Failed to load sessions', 'error');
    return;
  }

  renderSessions(res.sessions || []);
}

function renderSessions(sessions) {
  const list = document.getElementById('sessList');
  const revokeAllBtn = document.getElementById('revokeAllBtn');
  if (!list) return;

  if (sessions.length === 0) {
    list.innerHTML = `
      <div class="sess-empty">
        <div class="sess-empty-icon">
          <svg viewBox="0 0 24 24"><rect x="2" y="3" width="20" height="14" rx="2"/><path d="M8 21h8M12 17v4"/></svg>
        </div>
        <h3>No active sessions</h3>
        <p>You are not signed in on any device.</p>
      </div>`;
    if (revokeAllBtn) revokeAllBtn.style.display = 'none';
    return;
  }

  // Hide "sign out all others" if only 1 session (the current one)
  const otherCount = sessions.filter(s => !s.is_current).length;
  if (revokeAllBtn) revokeAllBtn.style.display = otherCount === 0 ? 'none' : '';

  list.innerHTML = sessions.map(s => buildCard(s)).join('');
}

function buildCard(s) {
  const isCurrent  = s.is_current;
  const device     = s.user_agent || 'Unknown Device';
  const location   = s.location   || 'Unknown Location';
  const loginTime  = formatDate(s.created_at);
  const expiry     = formatDate(s.expires_at);
  const sessionId  = s.session_id;

  return `
    <div class="sess-card ${isCurrent ? 'current' : ''}" id="card-${sessionId}">
      <div class="sess-device-icon">
        ${getDeviceIcon(device)}
      </div>

      <div class="sess-info">
        <div class="sess-info-top">
          <span class="sess-device-name">${escHtml(device)}</span>
          ${isCurrent ? '<span class="sess-current-badge">Current session</span>' : ''}
        </div>
        <div class="sess-meta">
          <span class="sess-meta-item">
            <svg viewBox="0 0 24 24"><path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"/><circle cx="12" cy="10" r="3"/></svg>
            ${escHtml(location)}
          </span>
          <span class="sess-meta-item">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>
            Signed in ${loginTime}
          </span>
          <span class="sess-meta-item">
            <svg viewBox="0 0 24 24"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>
            Expires ${expiry}
          </span>
        </div>
      </div>

      ${!isCurrent ? `
        <button class="btn-revoke" id="revoke-${sessionId}" onclick="revokeOne('${sessionId}')">
          <svg viewBox="0 0 24 24"><path d="M18 6L6 18M6 6l12 12"/></svg>
          Sign out
        </button>
      ` : ''}
    </div>`;
}

/* ----------------------------------------------------------
   Revoke single session
---------------------------------------------------------- */
async function revokeOne(sessionId) {
  const btn = document.getElementById(`revoke-${sessionId}`);
  if (btn) { btn.disabled = true; btn.textContent = 'Signing out…'; }

  const res = await API.revokeSession(sessionId);

  if (res.success) {
    // Animate card out
    const card = document.getElementById(`card-${sessionId}`);
    if (card) {
      card.style.transition = 'opacity .3s, transform .3s';
      card.style.opacity    = '0';
      card.style.transform  = 'translateX(16px)';
      setTimeout(() => { card.remove(); checkEmpty(); }, 320);
    }
    showToast('Session signed out');
  } else {
    if (btn) { btn.disabled = false; btn.innerHTML = '<svg viewBox="0 0 24 24"><path d="M18 6L6 18M6 6l12 12"/></svg> Sign out'; }
    showToast(res.message || 'Failed to sign out session', 'error');
  }
}

/* ----------------------------------------------------------
   Revoke all other sessions
---------------------------------------------------------- */
async function revokeAll() {
  const btn = document.getElementById('revokeAllBtn');
  if (btn) { btn.disabled = true; btn.textContent = 'Signing out…'; }

  const res = await API.revokeAllSessions();

  if (res.success) {
    showToast('All other sessions signed out');
    await loadSessions(); // refresh list
  } else {
    if (btn) btn.disabled = false;
    showToast(res.message || 'Failed to revoke sessions', 'error');
  }
}

/* ----------------------------------------------------------
   Check if list is now empty after removals
---------------------------------------------------------- */
function checkEmpty() {
  const list  = document.getElementById('sessList');
  const cards = list?.querySelectorAll('.sess-card');
  const revokeAllBtn = document.getElementById('revokeAllBtn');

  if (!cards || cards.length === 0) {
    if (list) list.innerHTML = `
      <div class="sess-empty">
        <div class="sess-empty-icon">
          <svg viewBox="0 0 24 24"><rect x="2" y="3" width="20" height="14" rx="2"/><path d="M8 21h8M12 17v4"/></svg>
        </div>
        <h3>No active sessions</h3>
        <p>You are not signed in on any other device.</p>
      </div>`;
    if (revokeAllBtn) revokeAllBtn.style.display = 'none';
    return;
  }

  // Hide button if only current session remains
  const others = list.querySelectorAll('.sess-card:not(.current)');
  if (revokeAllBtn) revokeAllBtn.style.display = others.length === 0 ? 'none' : '';
}

/* ----------------------------------------------------------
   Device icon helper — picks desktop, mobile, or tablet SVG
---------------------------------------------------------- */
function getDeviceIcon(device) {
  const d = device.toLowerCase();
  if (d.includes('android') || d.includes('iphone')) {
    // Mobile
    return `<svg viewBox="0 0 24 24"><rect x="5" y="2" width="14" height="20" rx="2" ry="2"/><line x1="12" y1="18" x2="12.01" y2="18"/></svg>`;
  }
  if (d.includes('ipad')) {
    // Tablet
    return `<svg viewBox="0 0 24 24"><rect x="4" y="2" width="16" height="20" rx="2" ry="2"/><line x1="12" y1="18" x2="12.01" y2="18"/></svg>`;
  }
  // Desktop / default
  return `<svg viewBox="0 0 24 24"><rect x="2" y="3" width="20" height="14" rx="2"/><path d="M8 21h8M12 17v4"/></svg>`;
}

/* ----------------------------------------------------------
   Loading
---------------------------------------------------------- */
function showLoading(show) {
  const el = document.getElementById('sessLoading');
  if (el) el.style.display = show ? 'block' : 'none';
}

/* ----------------------------------------------------------
   Toast
---------------------------------------------------------- */
let toastTimer = null;
function showToast(msg, type = 'success') {
  const t = document.getElementById('sessToast');
  if (!t) return;
  t.querySelector('.toast-msg').textContent = msg;
  t.className = 'sess-toast show' + (type === 'error' ? ' error' : '');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 3500);
}

/* ----------------------------------------------------------
   Helpers
---------------------------------------------------------- */
function formatDate(d) {
  if (!d) return '—';
  return new Date(d).toLocaleString('en-GB', {
    day: 'numeric', month: 'short', year: 'numeric',
    hour: '2-digit', minute: '2-digit'
  });
}

function escHtml(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
