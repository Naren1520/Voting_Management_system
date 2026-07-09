
const params     = new URLSearchParams(location.search);
const electionId = params.get('id');

let allVoters    = [];
let voterFilter  = 'all';
let doughnutInst = null;
let barInst      = null;
let _electionData = null; // cached election for schedule

/* ─────────────────────────────────────────────────────
   Init
───────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', async () => {
  if (!Auth.requireAuth()) return;
  if (!electionId) { window.location.href = '/dashboard/index.html'; return; }

  const res = await API.getElection(electionId);
  if (!res.success) { window.location.href = '/dashboard/index.html'; return; }

  const e = res.election;
  _electionData = e;

  // Guard: redirect multi elections to the correct page
  if (e.election_type === 'multi') {
    window.location.href = `/election/manage-multi.html?id=${electionId}`;
    return;
  }

  document.title = `${e.title} — VoteStack`;
  document.getElementById('electionTitle').textContent = e.title;
  document.getElementById('electionMeta').textContent  = 'Created ' + fmt(e.created_at);

  const badge = document.getElementById('electionStatus');
  badge.textContent = e.is_active ? 'Active' : 'Closed';
  badge.className   = 'status-badge ' + (e.is_active ? 'active' : 'closed');

  const link = `${location.origin}/vote/index.html?election=${electionId}`;
  document.getElementById('shareUrl').textContent = link;
  document.getElementById('shareUrl')._link = link;

  // Show election ID
  document.getElementById('electionIdDisplay').textContent = electionId;

  // Set face toggle initial state
  setFaceToggle(!!e.face_verify_enabled);

  // Build schedule widget with saved values
  Schedule.buildWidget('manageSchedWidget');
  Schedule.setValue('manageSchedWidget', e);
  Schedule.renderStatusBanner(e, document.getElementById('schedStatusWrap'));

  await loadAll();
});

async function loadAll() {
  await Promise.all([loadCandidates(), loadVoters(), loadResults()]);
}

/* ─────────────────────────────────────────────────────
   TAB SWITCHING
───────────────────────────────────────────────────── */
function switchTab(name, btn) {
  document.querySelectorAll('.mn-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.mn-panel').forEach(p => p.classList.remove('active'));
  btn.classList.add('active');
  const panelId = {
    candidates: 'panelCandidates',
    voters:     'panelVoters',
    results:    'panelResults',
    schedule:   'panelSchedule'
  }[name];
  document.getElementById(panelId)?.classList.add('active');
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
    // Re-render status banner with new values
    Schedule.renderStatusBanner(sched, document.getElementById('schedStatusWrap'));
  } else {
    showMsg('schedMsg', res.message || 'Failed to save schedule.', 'error');
  }
}

/* ─────────────────────────────────────────────────────
   CANDIDATES
───────────────────────────────────────────────────── */
async function loadCandidates() {
  const [res, resultsRes] = await Promise.all([
    API.getCandidates(electionId),
    API.getResults(electionId)
  ]);
  const list  = document.getElementById('candidatesList');
  const cands = res.candidates || [];

  // Merge vote counts from results into candidate list
  const voteCounts = {};
  (resultsRes.candidates || []).forEach(c => { voteCounts[c.name] = c.votes || 0; });
  cands.forEach(c => { c.votes = voteCounts[c.name] || 0; });

  document.getElementById('statCandidates').textContent = cands.length;
  document.getElementById('tcCand').textContent         = cands.length;

  if (!res.success || !cands.length) {
    list.innerHTML = emptyState(
      '<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/>',
      'No candidates yet',
      'Add your first candidate above to get started.'
    );
    return;
  }

  const sorted = [...cands].sort((a, b) => (b.votes||0) - (a.votes||0));
  list.innerHTML = sorted.map((c, i) => `
    <div class="candidate-card">
      <div class="candidate-avatar">${esc(c.name).charAt(0).toUpperCase()}</div>
      <div class="candidate-info">
        <strong>${esc(c.name)}</strong>
        <span>${c.votes ?? 0} vote${(c.votes ?? 0) !== 1 ? 's' : ''}</span>
      </div>
      <div class="candidate-rank">#${i + 1}</div>
      <button class="btn-danger-icon" onclick="deleteCandidate('${esc(c.name)}')" title="Remove candidate">
        <svg viewBox="0 0 24 24"><polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/><path d="M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2"/></svg>
      </button>
    </div>
  `).join('');
}

async function addCandidate() {
  const name = document.getElementById('candName').value.trim();
  if (!name) { showMsg('candMsg', 'Please enter a candidate name.', 'error'); return; }
  const res = await API.addCandidate(electionId, name);
  if (res.success) {
    document.getElementById('candName').value = '';
    showMsg('candMsg', `"${name}" added successfully.`, 'success');
    loadCandidates();
  } else {
    showMsg('candMsg', res.message || 'Could not add candidate.', 'error');
  }
}

async function deleteCandidate(name) {
  if (!confirm(`Remove "${name}" from this election?`)) return;
  await API.deleteCandidate(electionId, name);
  loadCandidates();
  toast('Candidate removed.');
}

/* 
   VOTERS
 */
async function loadVoters() {
  // Fetch registered voters and voted IDs in parallel
  const [voterRes, votedRes] = await Promise.all([
    API.getVoters(electionId),
    API.getVotedIds(electionId)
  ]);

  const voters   = voterRes.voters   || [];
  const votedIds = new Set(votedRes.voted_ids || []);

  allVoters = voters.map(v => ({
    ...v,
    has_voted: votedIds.has(v.voter_id)
  }));

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

  // Apply voted/pending filter
  if (voterFilter === 'voted')   filtered = filtered.filter(v => v.has_voted);
  if (voterFilter === 'pending') filtered = filtered.filter(v => !v.has_voted);

  // Apply search on top
  if (q) {
    filtered = filtered.filter(v =>
      v.name.toLowerCase().includes(q) ||
      v.voter_id.toLowerCase().includes(q) ||
      (v.email || '').toLowerCase().includes(q)
    );
  }

  renderVoters(filtered);
}

function renderVoters(voters) {
  const list = document.getElementById('votersList');
  if (!voters.length) {
    list.innerHTML = emptyState(
      '<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>',
      voterFilter === 'voted'   ? 'No one has voted yet' :
      voterFilter === 'pending' ? 'Everyone has voted!' :
                                  'No voters registered',
      voterFilter === 'voted'   ? 'Votes will appear here once voters cast their ballots.' :
      voterFilter === 'pending' ? 'All registered voters have submitted their votes.' :
                                  'Add voters above so they can participate in this election.'
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
  `).join('');
}

function filterVoters() {
  applyVoterFilter();
}

async function addVoter() {
  const voter_id = document.getElementById('vVoterId').value.trim();
  const name     = document.getElementById('vName').value.trim();
  const email    = document.getElementById('vEmail').value.trim();
  const phone    = document.getElementById('vPhone').value.trim();
  if (!voter_id || !name) { showMsg('voterMsg', 'Voter ID and name are required.', 'error'); return; }
  const res = await API.addVoter(electionId, { voter_id, name, email, phone });
  if (res.success) {
    ['vVoterId', 'vName', 'vEmail', 'vPhone'].forEach(id => document.getElementById(id).value = '');
    showMsg('voterMsg', `${name} added successfully.`, 'success');
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

/* 
   RESULTS & CHARTS
 */
const CHART_COLORS = [
  '#0067b8', '#60a5fa', '#16a34a', '#4ade80',
  '#d97706', '#fb923c', '#9333ea', '#f472b6',
  '#06b6d4', '#84cc16'
];

async function loadResults() {
  const res    = await API.getResults(electionId);
  const cands  = res.candidates || [];
  const total  = res.total_votes || 0;
  const voters = allVoters.length || 0;

  document.getElementById('totalVotes').textContent  = total;
  document.getElementById('statVotes').textContent   = total;
  document.getElementById('statTurnout').textContent =
    voters > 0 ? Math.round((total / voters) * 100) + '%' : '—';

  const list = document.getElementById('resultsList');

  if (!res.success || !cands.length) {
    list.innerHTML = emptyState(
      '<line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/>',
      'No results yet',
      'Results will appear here once voters start submitting ballots.'
    );
    renderCharts([], []);
    return;
  }

  const sorted   = [...cands].sort((a, b) => b.votes - a.votes);
  const maxVotes = sorted[0].votes || 1;

  list.innerHTML = sorted.map((c, i) => {
    const pct    = total > 0 ? Math.round((c.votes / total) * 100) : 0;
    const barPct = Math.round((c.votes / maxVotes) * 100);
    const isWin  = i === 0 && c.votes > 0;
    return `
      <div class="result-row">
        <div class="result-row-header">
          <div class="result-name">
            ${esc(c.name)}
            ${isWin ? '<span class="winner-crown">👑 Leading</span>' : ''}
          </div>
          <div class="result-meta">${c.votes} vote${c.votes !== 1 ? 's' : ''} · ${pct}%</div>
        </div>
        <div class="result-bar-bg">
          <div class="result-bar-fill ${isWin ? 'winner' : ''}" style="width:${barPct}%"></div>
        </div>
      </div>
    `;
  }).join('');

  renderCharts(sorted.map(c => c.name), sorted.map(c => c.votes));
}

function renderCharts(labels, data) {
  /* Doughnut — distribution */
  if (doughnutInst) doughnutInst.destroy();
  const dc = document.getElementById('doughnutChart').getContext('2d');
  doughnutInst = new Chart(dc, {
    type: 'doughnut',
    data: {
      labels,
      datasets: [{
        data,
        backgroundColor: CHART_COLORS.slice(0, labels.length),
        borderWidth: 0,
        hoverOffset: 8
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          position: 'bottom',
          labels: { font: { family: 'Inter', size: 12 }, padding: 14, boxWidth: 12, color: '#3d3d3d' }
        },
        tooltip: {
          callbacks: { label: ctx => ` ${ctx.label}: ${ctx.parsed} votes` }
        }
      },
      cutout: '62%'
    }
  });

  /* Horizontal bar — vote count per candidate */
  if (barInst) barInst.destroy();
  const bc = document.getElementById('barChart').getContext('2d');
  barInst = new Chart(bc, {
    type: 'bar',
    data: {
      labels,
      datasets: [{
        label: 'Votes',
        data,
        backgroundColor: CHART_COLORS.slice(0, labels.length),
        borderRadius: 6,
        borderSkipped: false
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      indexAxis: 'y',
      plugins: {
        legend: { display: false },
        tooltip: { callbacks: { label: ctx => ` ${ctx.parsed.x} votes` } }
      },
      scales: {
        x: {
          grid: { color: '#ebebeb' },
          ticks: { font: { family: 'Inter', size: 11 }, color: '#9e9e9e' }
        },
        y: {
          grid: { display: false },
          ticks: { font: { family: 'Inter', size: 12, weight: '600' }, color: '#3d3d3d' }
        }
      }
    }
  });
}

/* 
   HELPERS
 */
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
    toast('Failed to update face verification setting', 'error');
  }
}

function showMsg(id, text, type) {
  const el = document.getElementById(id);
  el.textContent   = text;
  el.className     = 'mn-msg ' + type;
  el.style.display = 'block';
  setTimeout(() => { el.style.display = 'none'; }, 4500);
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
  return `
    <div class="mn-empty">
      <div class="mn-empty-icon">
        <svg viewBox="0 0 24 24" style="width:24px;height:24px;stroke:#0067b8;fill:none;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:round">${iconPath}</svg>
      </div>
      <h3>${title}</h3>
      <p>${sub}</p>
    </div>`;
}

function esc(s) {
  return String(s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;')
    .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function fmt(d) {
  return new Date(d).toLocaleDateString('en-IN', { day: 'numeric', month: 'short', year: 'numeric' });
}
