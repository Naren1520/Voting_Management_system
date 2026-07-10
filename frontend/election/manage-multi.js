const params     = new URLSearchParams(location.search);
const electionId = params.get('id');

let allPositions = [];
let allVoters    = [];
let voterFilter  = 'all';
let doughnutInsts = {};

/* ─────────────────────────────────────────────────────
   Init
───────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', async () => {
  if (!Auth.requireAuth()) return;
  if (!electionId) { window.location.href = '/dashboard/index.html'; return; }

  const res = await API.getElection(electionId);
  if (!res.success) { window.location.href = '/dashboard/index.html'; return; }

  const e = res.election;

  // Guard: redirect standard elections to the correct page
  if (e.election_type && e.election_type !== 'multi') {
    window.location.href = `/election/manage.html?id=${electionId}`;
    return;
  }
  document.title = `${e.title} — VoteStack`;
  document.getElementById('electionTitle').textContent = e.title;
  document.getElementById('electionMeta').textContent  = 'Created ' + fmt(e.created_at);

  const badge = document.getElementById('electionStatus');
  badge.textContent = e.is_active ? 'Active' : 'Closed';
  badge.className   = 'status-badge ' + (e.is_active ? 'active' : 'closed');

  const link = `${location.origin}/vote-multi/index.html?election=${electionId}`;
  const pill = document.getElementById('shareUrl');
  pill.textContent = link;
  pill._link = link;

  // Show election ID
  document.getElementById('electionIdDisplay').textContent = electionId;

  // Set face toggle initial state
  setFaceToggle(!!e.face_verify_enabled);

  // Schedule widget
  Schedule.buildWidget('manageSchedWidget');
  Schedule.setValue('manageSchedWidget', e);
  Schedule.renderStatusBanner(e, document.getElementById('schedStatusWrap'));

  await loadAll();

  hideSkeleton();
});

async function loadAll() {
  await Promise.all([loadPositions(), loadVoters(), loadResults()]);
}

/* ─────────────────────────────────────────────────────
   TAB SWITCHING
───────────────────────────────────────────────────── */
function switchTab(name, btn) {
  document.querySelectorAll('.mn-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.mn-panel').forEach(p => p.classList.remove('active'));
  btn.classList.add('active');
  const map = {
    positions: 'panelPositions',
    voters:    'panelVoters',
    results:   'panelResults',
    schedule:  'panelSchedule'
  };
  document.getElementById(map[name])?.classList.add('active');
}

/* ─────────────────────────────────────────────────────
   SCHEDULE
───────────────────────────────────────────────────── */
async function saveSchedule() {
  const err = Schedule.validate('manageSchedWidget');
  if (err) { showMsg('schedMsg', err, 'error'); return; }

  const sched = Schedule.getValue('manageSchedWidget');
  const res   = await API.updateSchedule(electionId, sched);

  if (res.success) {
    showMsg('schedMsg', 'Schedule saved.', 'success');
    Schedule.renderStatusBanner(sched, document.getElementById('schedStatusWrap'));
  } else {
    showMsg('schedMsg', res.message || 'Failed to save schedule.', 'error');
  }
}

/* ─────────────────────────────────────────────────────
   POSITIONS & CANDIDATES
───────────────────────────────────────────────────── */
async function loadPositions() {
  const [posRes, resultsRes] = await Promise.all([
    API.getPositions(electionId),
    API.getMultiResults(electionId)
  ]);
  allPositions = posRes.positions || [];

  document.getElementById('statPositions').textContent = allPositions.length;
  document.getElementById('tcPos').textContent         = allPositions.length;

  // Build vote count map: positionId -> { candidateName -> votes }
  const voteMap = {};
  (resultsRes.positions || []).forEach(pos => {
    voteMap[pos.id] = {};
    (pos.candidates || []).forEach(c => { voteMap[pos.id][c.name] = c.votes || 0; });
  });

  // Load candidates for each position in parallel
  const candResults = await Promise.all(
    allPositions.map(p => API.getPosCandidates(electionId, p.id))
  );
  allPositions.forEach((p, i) => {
    p.candidates = (candResults[i].candidates || []).map(c => ({
      ...c,
      votes: (voteMap[p.id] && voteMap[p.id][c.name]) || 0
    }));
  });

  const totalCands = allPositions.reduce((s, p) => s + p.candidates.length, 0);
  document.getElementById('statCandidates').textContent = totalCands;

  renderPositionsAccordion();
}

function renderPositionsAccordion() {
  const wrap = document.getElementById('positionsAccordion');
  if (!allPositions.length) {
    wrap.innerHTML = emptyState(
      '<path d="M12 2l3.09 6.26L22 9.27l-5 4.87 1.18 6.88L12 17.77l-6.18 3.25L7 14.14 2 9.27l6.91-1.01L12 2z"/>',
      'No positions yet',
      'Add positions using the button above.'
    );
    return;
  }

  wrap.innerHTML = allPositions.map((pos, idx) => `
    <div class="pos-card" id="posCard_${esc(pos.id)}">
      <div class="pos-card-header" onclick="togglePos('${esc(pos.id)}')">
        <div class="pos-card-left">
          <div class="pos-num">${idx + 1}</div>
          <div>
            <div class="pos-title">${esc(pos.title)}</div>
            <div class="pos-meta">${pos.candidates.length} candidate${pos.candidates.length !== 1 ? 's' : ''}</div>
          </div>
        </div>
        <div class="pos-card-right">
          <button class="btn-danger-icon" onclick="event.stopPropagation(); deletePosition('${esc(pos.id)}','${esc(pos.title)}')" title="Remove position">
            <svg viewBox="0 0 24 24"><polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6M14 11v6"/><path d="M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2"/></svg>
          </button>
          <svg class="pos-chevron" viewBox="0 0 24 24"><polyline points="6 9 12 15 18 9"/></svg>
        </div>
      </div>
      <div class="pos-card-body" id="posBody_${esc(pos.id)}">
        <div class="add-form-row" style="margin-bottom:16px;">
          <input id="candInput_${esc(pos.id)}" class="mn-input" type="text" placeholder="Candidate name"
            onkeydown="if(event.key==='Enter')addCandidate('${esc(pos.id)}')"/>
          <button class="btn-primary" onclick="addCandidate('${esc(pos.id)}')">
            <svg viewBox="0 0 24 24"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            Add
          </button>
        </div>
        <div id="candList_${esc(pos.id)}">
          ${renderCandidateList(pos.candidates, pos.id)}
        </div>
      </div>
    </div>
  `).join('');

  // Open all by default
  allPositions.forEach(p => openPos(p.id));
}

function renderCandidateList(candidates, posId) {
  if (!candidates.length) return `
    <div class="pos-empty">No candidates yet — add one above.</div>`;
  return candidates.map((c, i) => `
    <div class="candidate-card">
      <div class="candidate-avatar">${esc(c.name).charAt(0).toUpperCase()}</div>
      <div class="candidate-info">
        <strong>${esc(c.name)}</strong>
        <span>${c.votes ?? 0} vote${(c.votes ?? 0) !== 1 ? 's' : ''}</span>
      </div>
      <div class="candidate-rank">#${i + 1}</div>
      <button class="btn-danger-icon" onclick="deleteCandidate('${esc(posId)}','${esc(c.name)}')" title="Remove">
        <svg viewBox="0 0 24 24"><polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6M14 11v6"/><path d="M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2"/></svg>
      </button>
    </div>
  `).join('');
}

function togglePos(posId) {
  const body = document.getElementById(`posBody_${posId}`);
  const card = document.getElementById(`posCard_${posId}`);
  const isOpen = card.classList.contains('open');
  if (isOpen) { card.classList.remove('open'); body.style.display = 'none'; }
  else         { card.classList.add('open');    body.style.display = 'block'; }
}

function openPos(posId) {
  const body = document.getElementById(`posBody_${posId}`);
  const card = document.getElementById(`posCard_${posId}`);
  if (body) body.style.display = 'block';
  if (card) card.classList.add('open');
}

/* ─────────────────────────────────────────────────────
   ADD / DELETE POSITION
───────────────────────────────────────────────────── */
function openAddPositionModal() {
  document.getElementById('newPosTitle').value = '';
  document.getElementById('addPosOverlay').classList.add('open');
  setTimeout(() => document.getElementById('newPosTitle').focus(), 50);
}
function closeAddPositionModal() {
  document.getElementById('addPosOverlay').classList.remove('open');
  hideMsg('addPosMsg');
}

async function submitAddPosition() {
  const title = document.getElementById('newPosTitle').value.trim();
  if (!title) { showMsg('addPosMsg', 'Enter a position title.', 'error'); return; }
  const res = await API.addPosition(electionId, title);
  if (res.success) {
    closeAddPositionModal();
    toast('Position added.');
    await loadPositions();
  } else {
    showMsg('addPosMsg', res.message || 'Failed to add position.', 'error');
  }
}

async function deletePosition(posId, title) {
  if (!confirm(`Remove position "${title}" and all its candidates?`)) return;
  await API.deletePosition(electionId, posId);
  toast('Position removed.');
  await loadPositions();
}

/* ─────────────────────────────────────────────────────
   ADD / DELETE CANDIDATE
───────────────────────────────────────────────────── */
async function addCandidate(posId) {
  const input = document.getElementById(`candInput_${posId}`);
  const name  = input.value.trim();
  if (!name) return;
  const res = await API.addPosCandidate(electionId, posId, name);
  if (res.success) {
    input.value = '';
    // Update local data + re-render just that list
    const pos = allPositions.find(p => p.id === posId);
    if (pos) {
      pos.candidates = res.candidates || [];
      document.getElementById(`candList_${posId}`).innerHTML =
        renderCandidateList(pos.candidates, posId);
      document.getElementById('statCandidates').textContent =
        allPositions.reduce((s, p) => s + p.candidates.length, 0);
    }
    toast(`"${name}" added.`);
  } else {
    showMsg('posMsg', res.message || 'Could not add candidate.', 'error');
  }
}

async function deleteCandidate(posId, name) {
  if (!confirm(`Remove "${name}"?`)) return;
  const res = await API.delPosCandidate(electionId, posId, name);
  if (res.success) {
    const pos = allPositions.find(p => p.id === posId);
    if (pos) {
      pos.candidates = res.candidates || [];
      document.getElementById(`candList_${posId}`).innerHTML =
        renderCandidateList(pos.candidates, posId);
      document.getElementById('statCandidates').textContent =
        allPositions.reduce((s, p) => s + p.candidates.length, 0);
    }
    toast('Candidate removed.');
  }
}

/* ─────────────────────────────────────────────────────
   VOTERS  (identical logic to standard manage.js)
───────────────────────────────────────────────────── */
async function loadVoters() {
  const [voterRes, votedRes] = await Promise.all([
    API.getVoters(electionId),
    API.getVotedIds(electionId)
  ]);
  const voters   = voterRes.voters   || [];
  const votedIds = new Set(votedRes.voted_ids || []);

  allVoters = voters.map(v => ({ ...v, has_voted: votedIds.has(v.voter_id) }));
  document.getElementById('statVoters').textContent = allVoters.length;
  document.getElementById('tcVoter').textContent    = allVoters.length;
  updateFilterCounts();
  applyVoterFilter();
}

function updateFilterCounts() {
  const voted   = allVoters.filter(v => v.has_voted).length;
  const pending = allVoters.length - voted;
  document.getElementById('fpAllCount').textContent     = allVoters.length;
  document.getElementById('fpVotedCount').textContent   = voted;
  document.getElementById('fpPendingCount').textContent = pending;
}

function setVoterFilter(filter, btn) {
  voterFilter = filter;
  document.querySelectorAll('.filter-pill').forEach(p => p.classList.remove('active'));
  btn.classList.add('active');
  applyVoterFilter();
}

function applyVoterFilter() {
  const q = (document.getElementById('voterSearch').value || '').toLowerCase();
  let filtered = allVoters;
  if (voterFilter === 'voted')   filtered = filtered.filter(v => v.has_voted);
  if (voterFilter === 'pending') filtered = filtered.filter(v => !v.has_voted);
  if (q) filtered = filtered.filter(v =>
    v.name.toLowerCase().includes(q) ||
    v.voter_id.toLowerCase().includes(q) ||
    (v.email || '').toLowerCase().includes(q)
  );
  renderVoters(filtered);
}

function renderVoters(voters) {
  const list = document.getElementById('votersList');
  if (!voters.length) {
    list.innerHTML = emptyState(
      '<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>',
      voterFilter === 'voted'   ? 'No one has voted yet' :
      voterFilter === 'pending' ? 'Everyone has voted!' : 'No voters registered',
      voterFilter === 'voted'   ? 'Votes will appear here once voters cast their ballots.' :
      voterFilter === 'pending' ? 'All registered voters have submitted their votes.' :
                                  'Add voters above so they can participate.'
    );
    return;
  }
  list.innerHTML = voters.map(v => `
    <div class="voter-card ${v.has_voted ? 'voter-card--voted' : ''}">
      <div class="voter-avatar ${v.has_voted ? 'voted' : ''}">${esc(v.name).charAt(0).toUpperCase()}</div>
      <div class="voter-info">
        <strong>${esc(v.name)}</strong>
        <small>${[v.email, v.phone].filter(Boolean).map(esc).join(' · ') || 'No contact info'}</small>
      </div>
      <div class="voter-meta">
        <span class="voter-id-chip">${esc(v.voter_id)}</span>
        ${v.has_voted
          ? '<span class="voted-chip"><svg viewBox="0 0 24 24" style="width:10px;height:10px;stroke:currentColor;fill:none;stroke-width:3;stroke-linecap:round;stroke-linejoin:round"><polyline points="20 6 9 17 4 12"/></svg>Voted</span>'
          : '<span class="pending-chip"><svg viewBox="0 0 24 24" style="width:10px;height:10px;stroke:currentColor;fill:none;stroke-width:2.5;stroke-linecap:round;stroke-linejoin:round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>Pending</span>'}
        <a class="btn-face-enroll" href="/face-enroll/index.html?election=${electionId}&voter=${encodeURIComponent(v.voter_id)}" title="Enroll face">
          <svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="4"/><path d="M4 20c0-4 3.6-7 8-7s8 3 8 7"/><polyline points="16 11 18 13 22 9"/></svg>
        </a>
        <button class="btn-danger-icon" onclick="deleteVoter('${esc(v.voter_id)}')" title="Remove voter">
          <svg viewBox="0 0 24 24"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>
        </button>
      </div>
    </div>
  `).join('');
}

function filterVoters() { applyVoterFilter(); }

async function addVoter() {
  const voter_id = document.getElementById('vVoterId').value.trim();
  const name     = document.getElementById('vName').value.trim();
  const email    = document.getElementById('vEmail').value.trim();
  const phone    = document.getElementById('vPhone').value.trim();
  if (!voter_id || !name) { showMsg('voterMsg', 'Voter ID and name are required.', 'error'); return; }
  const res = await API.addVoter(electionId, { voter_id, name, email, phone });
  if (res.success) {
    ['vVoterId','vName','vEmail','vPhone'].forEach(id => document.getElementById(id).value = '');
    showMsg('voterMsg', `${name} added.`, 'success');
    loadVoters();
  } else {
    showMsg('voterMsg', res.message || 'Could not add voter.', 'error');
  }
}

async function deleteVoter(voterId) {
  if (!confirm(`Remove voter "${voterId}"?`)) return;
  await API.deleteVoter(electionId, voterId);
  loadVoters();
  toast('Voter removed.');
}

/* ─────────────────────────────────────────────────────
   RESULTS
───────────────────────────────────────────────────── */
async function loadResults() {
  const res = await API.getMultiResults(electionId);
  const positions = res.positions || [];
  const voters    = allVoters.length;

  const totalVoted = positions.reduce((s, p) => s + (p.total_votes || 0), 0);
  const avgTurnout = positions.length > 0 && voters > 0
    ? Math.round((totalVoted / positions.length / voters) * 100)
    : 0;

  document.getElementById('totalPositionsResult').textContent =
    positions.filter(p => (p.total_votes || 0) > 0).length;
  document.getElementById('statTurnout').textContent =
    positions.length > 0 ? avgTurnout + '%' : '—';

  const wrap = document.getElementById('multiResultsList');
  if (!positions.length) {
    wrap.innerHTML = emptyState(
      '<line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/>',
      'No results yet',
      'Results appear once voters start casting votes.'
    );
    return;
  }

  wrap.innerHTML = positions.map(pos => {
    const total   = pos.total_votes || 0;
    const cands   = pos.candidates  || [];
    const sorted  = [...cands].sort((a, b) => b.votes - a.votes);
    const maxVotes= sorted[0]?.votes || 1;
    const turnout = voters > 0 ? Math.round((total / voters) * 100) : 0;

    const rows = sorted.map((c, i) => {
      const pct    = total > 0 ? Math.round((c.votes / total) * 100) : 0;
      const barPct = Math.round((c.votes / maxVotes) * 100);
      const isWin  = i === 0 && c.votes > 0;
      return `
        <div class="result-row">
          <div class="result-row-header">
            <div class="result-name">${esc(c.name)}
              ${isWin ? '<span class="winner-crown">👑 Leading</span>' : ''}
            </div>
            <div class="result-meta">${c.votes} vote${c.votes!==1?'s':''} · ${pct}%</div>
          </div>
          <div class="result-bar-bg">
            <div class="result-bar-fill ${isWin ? 'winner' : ''}" style="width:${barPct}%"></div>
          </div>
        </div>`;
    }).join('');

    return `
      <div class="pos-result-card">
        <div class="pos-result-header">
          <div class="pos-result-title">${esc(pos.title)}</div>
          <div class="pos-result-meta">
            ${total} vote${total!==1?'s':''} · ${turnout}% turnout
          </div>
        </div>
        <div class="pos-result-body">
          ${rows || '<div class="pos-empty">No votes yet.</div>'}
        </div>
      </div>`;
  }).join('');
}

/* ─────────────────────────────────────────────────────
   HELPERS
───────────────────────────────────────────────────── */
function copyLink() {
  const el   = document.getElementById('shareUrl');
  const link = el._link || el.textContent;
  navigator.clipboard.writeText(link).then(() => toast('Voting link copied!'));
}

function copyElectionId() {
  navigator.clipboard.writeText(electionId).then(() => toast('Election ID copied!'));
}

/* ─────────────────────────────────────────────────────
   FACE VERIFICATION TOGGLE
───────────────────────────────────────────────────── */
let _faceVerifyEnabled = false;

function setFaceToggle(enabled) {
  _faceVerifyEnabled = enabled;
  const el = document.getElementById('faceToggle');
  if (el) el.classList.toggle('on', enabled);
}

async function toggleFaceVerify() {
  const newVal = !_faceVerifyEnabled;
  const res = await API.toggleFaceVerify(electionId, newVal);
  if (res.success) {
    setFaceToggle(newVal);
    toast(newVal ? 'Face verification enabled' : 'Face verification disabled');
  } else {
    toast('Failed to update face verification setting');
  }
}

function showMsg(id, text, type) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent   = text;
  el.className     = 'mn-msg ' + type;
  el.style.display = 'block';
  setTimeout(() => { el.style.display = 'none'; }, 4500);
}
function hideMsg(id) {
  const el = document.getElementById(id);
  if (el) el.style.display = 'none';
}

function toast(msg) {
  const t = document.getElementById('mnToast');
  document.getElementById('mnToastMsg').textContent = msg;
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 3200);
}

async function handleLogout() {
  await API.logout();
  Auth.clear();
  window.location.href = '/landing/index.html';
}

function emptyState(iconPath, title, sub) {
  return `<div class="mn-empty">
    <div class="mn-empty-icon">
      <svg viewBox="0 0 24 24" style="width:24px;height:24px;stroke:#9333ea;fill:none;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:round">${iconPath}</svg>
    </div>
    <h3>${title}</h3><p>${sub}</p>
  </div>`;
}

function esc(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function fmt(d) {
  return new Date(d).toLocaleDateString('en-IN', { day:'numeric', month:'short', year:'numeric' });
}

function hideSkeleton() {
  const sk   = document.getElementById('pageSkeletonOverlay');
  const page = document.querySelector('.mn-page');

  if (page) page.classList.add('mn-ready');

  if (!sk) return;
  sk.classList.add('sk-hidden');
  sk.addEventListener('transitionend', () => sk.remove(), { once: true });
}
