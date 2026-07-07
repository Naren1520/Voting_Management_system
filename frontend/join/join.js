/* ============================================================
   join.js — VoteStack election entry page
   ============================================================ */

async function joinElection() {
  const raw = document.getElementById('electionIdInput').value.trim();
  const btn = document.getElementById('joinBtn');

  hideError();

  if (!raw) {
    showError('Please enter an Election ID.');
    return;
  }

  // Basic UUID format check
  const uuidLike = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  if (!uuidLike.test(raw)) {
    showError(
      'That doesn\'t look like a valid Election ID. ' +
      'It should be in the format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx'
    );
    return;
  }

  setLoading(btn, true);

  // Single call — backend now returns election_type in the response
  let infoRes;
  try {
    infoRes = await API.getElectionInfo(raw);
  } catch (_) {
    infoRes = { success: false };
  }

  setLoading(btn, false);

  if (!infoRes.success) {
    showError('Election not found. Check the ID and try again.');
    return;
  }

  if (!infoRes.is_active) {
    showError('This election is currently closed.');
    return;
  }

  // Route to correct ballot based on election_type from server
  const type = infoRes.election_type || 'standard';
  const dest = type === 'multi'
    ? '/vote-multi/index.html?election=' + encodeURIComponent(raw)
    : '/vote/index.html?election='       + encodeURIComponent(raw);

  window.location.href = dest;
}

/* ── Helpers ──────────────────────────────────────────────── */

function setLoading(btn, loading) {
  if (loading) {
    btn.disabled  = true;
    btn.innerHTML = '<span class="btn-spinner"></span> Checking…';
  } else {
    btn.disabled  = false;
    btn.innerHTML =
      'Open Ballot ' +
      '<svg viewBox="0 0 24 24">' +
        '<line x1="5" y1="12" x2="19" y2="12"/>' +
        '<polyline points="12 5 19 12 12 19"/>' +
      '</svg>';
  }
}

function showError(msg) {
  const el = document.getElementById('joinError');
  el.textContent   = msg;
  el.style.display = 'block';
}

function hideError() {
  const el = document.getElementById('joinError');
  if (el) el.style.display = 'none';
}
