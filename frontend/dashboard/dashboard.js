

document.addEventListener('DOMContentLoaded', () => {
  if (!Auth.requireAuth()) return;

  const user = Auth.user();
  const name = user?.name?.split(' ')[0] || 'there';
  const greetEl = document.getElementById('userGreet');
  if (greetEl) greetEl.textContent = 'Hi, ' + name;

  // Set profile avatar initials
  const avatarEl = document.getElementById('headerAvatar');
  if (avatarEl && user?.name) {
    const initials = user.name
      .split(' ')
      .map(w => w[0])
      .join('')
      .toUpperCase()
      .slice(0, 2);
    avatarEl.textContent = initials;
  }

  loadElections();
});

/* ----------------------------------------------------------
   State
---------------------------------------------------------- */
let elections = [];
let pendingDeleteId = null;

/* ----------------------------------------------------------
   Load + render elections
---------------------------------------------------------- */
async function loadElections() {
  showLoading(true);
  const res = await API.getElections();
  showLoading(false);

  if (!res.success) {
    showToast('Failed to load elections', 'error');
    return;
  }

  elections = res.elections || [];
  updateStats();
  renderElections();
}

function updateStats() {
  const total  = elections.length;
  const active = elections.filter(e => e.is_active).length;
  const closed = total - active;

  setEl('statTotal',  total);
  setEl('statActive', active);
  setEl('statClosed', closed);
}

function renderElections() {
  const grid = document.getElementById('electionsGrid');
  if (!grid) return;

  if (elections.length === 0) {
    grid.innerHTML = `
      <div class="dash-empty">
        <div class="dash-empty-icon">
          <svg viewBox="0 0 24 24"><rect x="3" y="3" width="18" height="18" rx="3"/><path d="M9 12h6M12 9v6"/></svg>
        </div>
        <h3>No elections yet</h3>
        <p>Create your first election to get started.</p>
        <button class="btn-new" onclick="openCreateModal()">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
          New Election
        </button>
      </div>`;
    return;
  }

  grid.innerHTML = elections.map(e => `
    <div class="election-card" onclick="goManage('${e.id}')">
      <div class="ec-top">
        <div class="ec-title">${escHtml(e.title)}</div>
        <span class="ec-badge ${e.is_active ? 'active' : 'closed'}">
          ${e.is_active ? 'Active' : 'Closed'}
        </span>
      </div>
      <div class="ec-date">
        <svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/></svg>
        ${formatDate(e.created_at)}
      </div>
      <div class="ec-divider"></div>
      <div class="ec-actions" onclick="event.stopPropagation()">
        <button class="ec-btn ec-btn-manage" onclick="goManage('${e.id}')">
          <svg viewBox="0 0 24 24"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>
          Manage
        </button>
        <button class="ec-btn ec-btn-copy" onclick="copyLink('${e.id}')">
          <svg viewBox="0 0 24 24"><rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></svg>
          Copy link
        </button>
        <button class="ec-btn-delete" onclick="confirmDelete('${e.id}','${escHtml(e.title)}')" aria-label="Delete election">
          <svg viewBox="0 0 24 24"><polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6M14 11v6"/><path d="M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2"/></svg>
        </button>
      </div>
    </div>
  `).join('');
}

/* ----------------------------------------------------------
   Navigation
---------------------------------------------------------- */
function goManage(id) {
  window.location.href = `/election/manage.html?id=${id}`;
}

function copyLink(id) {
  const link = `${window.location.origin}/vote/index.html?election=${id}`;
  navigator.clipboard.writeText(link).then(() => showToast('Voting link copied!'));
}

/* ----------------------------------------------------------
   Create election modal
---------------------------------------------------------- */
function openCreateModal() {
  document.getElementById('createOverlay').classList.add('open');
  setTimeout(() => document.getElementById('electionTitle').focus(), 50);
}

function closeCreateModal() {
  document.getElementById('createOverlay').classList.remove('open');
  document.getElementById('electionTitle').value = '';
  hideModalMsg('createMsg');
}

function closeModalOutside(e, modalId) {
  if (e.target === document.getElementById(modalId)) {
    if (modalId === 'createOverlay') closeCreateModal();
    if (modalId === 'deleteOverlay') closeDeleteModal();
  }
}

async function createElection() {
  const title = document.getElementById('electionTitle').value.trim();
  const btn   = document.getElementById('createBtn');

  if (!title) { showModalMsg('createMsg', 'Please enter a title.', 'error'); return; }

  btn.disabled = true;
  btn.innerHTML = '<span class="m-spinner"></span> Creating…';

  const res = await API.createElection(title);

  if (res.success) {
    closeCreateModal();
    showToast('Election created successfully');
    loadElections();
  } else {
    showModalMsg('createMsg', res.message || 'Failed to create election.', 'error');
    btn.disabled = false;
    btn.innerHTML = `Create <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/></svg>`;
  }
}

/* ----------------------------------------------------------
   Delete modal
---------------------------------------------------------- */
function confirmDelete(id, title) {
  pendingDeleteId = id;
  const el = document.getElementById('deleteTitle');
  if (el) el.textContent = title;
  document.getElementById('deleteOverlay').classList.add('open');
}

function closeDeleteModal() {
  document.getElementById('deleteOverlay').classList.remove('open');
  pendingDeleteId = null;
}

async function doDelete() {
  if (!pendingDeleteId) return;
  const btn = document.getElementById('confirmDeleteBtn');
  btn.disabled = true;
  btn.textContent = 'Deleting…';

  await API.deleteElection(pendingDeleteId);
  closeDeleteModal();
  showToast('Election deleted');
  loadElections();
}

/* ----------------------------------------------------------
   Logout
---------------------------------------------------------- */
async function handleLogout() {
  await API.logout();
  Auth.clear();
  window.location.href = '/landing/index.html';
}

/* ----------------------------------------------------------
   Toast
---------------------------------------------------------- */
let toastTimer = null;
function showToast(msg, type = 'success') {
  const t = document.getElementById('dashToast');
  if (!t) return;
  t.querySelector('.toast-msg').textContent = msg;
  t.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 3000);
}

/* ----------------------------------------------------------
   Modal message
---------------------------------------------------------- */
function showModalMsg(id, text, type) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = text;
  el.className = 'modal-msg ' + type;
  el.style.display = 'block';
}
function hideModalMsg(id) {
  const el = document.getElementById(id);
  if (el) el.style.display = 'none';
}

/* ----------------------------------------------------------
   Helpers
---------------------------------------------------------- */
function showLoading(show) {
  const el = document.getElementById('loadingState');
  if (el) el.style.display = show ? 'block' : 'none';
}

function setEl(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}

function escHtml(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function formatDate(d) {
  return new Date(d).toLocaleDateString('en-GB', {
    day: 'numeric', month: 'short', year: 'numeric'
  });
}
