/**
 * vote-multi.js
 * Stepped ballot for multi-position elections.
 * Flow: Verify voter → vote per position (one at a time) → review → submit
 */

const params     = new URLSearchParams(location.search);
const electionId = params.get('election');

let positions      = [];   // [{id, title, candidates:[{name}]}]
let selections     = {};   // { position_id: candidate_name }
let currentStep    = 0;    // index into positions[] during vote step
let currentVoterId = null;

/* ─────────────────────────────────────────────────────
   Init
───────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', async () => {
  if (!electionId) { showError('Invalid voting link.'); return; }

  // Load info + ballot in parallel
  const [infoRes, ballotRes] = await Promise.all([
    API.getMultiInfo(electionId),
    API.getMultiBallot(electionId)
  ]);

  hide('loadingMsg');

  if (!infoRes.success)   { showError(infoRes.message  || 'Election not found.'); return; }
  if (!ballotRes.success) { showError(ballotRes.message || 'Could not load ballot.'); return; }
  if (!infoRes.is_active) { showError('This election is closed.'); return; }

  document.getElementById('electionTitleDisplay').textContent = infoRes.title;
  document.getElementById('electionSubtitle').textContent     = 'Multi-position election';
  document.title = infoRes.title + ' - VoteStack';

  // Store face verify flag
  window._faceVerifyEnabled = !!infoRes.face_verify_enabled;

  //  Schedule check 
  const sched = {
    schedule_type: infoRes.schedule_type || 'always_on',
    timezone:      infoRes.timezone,
    starts_at:     infoRes.starts_at,
    ends_at:       infoRes.ends_at,
    schedule_json: infoRes.schedule_json
  };
  const status = Schedule.getStatus(sched);
  if (!status.open) {
    const block = document.getElementById('schedBlock');
    block.style.display = 'block';
    document.getElementById('schedBlockLabel').textContent  = status.label;
    document.getElementById('schedBlockReason').textContent = status.reason;
    if (status.nextChange) {
      Schedule.startCountdown(status.nextChange,
        document.getElementById('schedBlockCountdown'));
    }
    return;
  }
  // ─────────────────────────────────────────────────────────

  positions = ballotRes.positions || [];

  if (!positions.length) {
    showError('No positions have been added to this election yet.');
    return;
  }

  show('stepId');
});

/* 
   Step 1 - Verify voter
 */
async function checkVoter() {
  const voterId = document.getElementById('voterIdInput').value.trim();
  const btn     = document.getElementById('checkBtn');
  if (!voterId) { showMsg('Enter your voter ID.', 'error'); return; }

  btn.disabled  = true;
  btn.innerHTML = '<span class="spinner" style="width:16px;height:16px;display:inline-block;margin-right:8px;"></span>Verifying…';

  const res = await API.checkMultiVoter(electionId, voterId);

  btn.disabled  = false;
  btn.innerHTML = 'Verify &amp; Start Voting <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="margin-left:6px"><line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/></svg>';

  if (!res.success) {
    showMsg(res.message || 'Voter not found.', 'error');
    return;
  }
  if (res.fully_voted) {
    showMsg('You have already voted in all positions for this election.', 'error');
    return;
  }

  currentVoterId = voterId;
  selections     = {};
  currentStep    = 0;

  // Pre-mark positions already voted so we skip them
  const alreadyVoted = new Set(res.voted_positions || []);
  positions = positions.filter(p => !alreadyVoted.has(p.id));

  if (!positions.length) {
    showMsg('You have already voted in all available positions.', 'error');
    return;
  }

  // Go to face verification step (or skip if disabled)
  hide('stepId');
  hideMsg();
  document.getElementById('faceVoterLabel').textContent = voterId;

  if (!window._faceVerifyEnabled) {
    proceedToBallot();
    return;
  }

  show('stepFace');
  await openCamera();
}

/* ─────────────────────────────────────────────────────
   Step 1.5 - Face Verification
───────────────────────────────────────────────────── */
let _faceCapture = null;

async function openCamera() {
  if (!window.isSecureContext) {
    showMsg('Camera requires HTTPS. If testing locally, use http://localhost - not an IP address.', 'error');
    return;
  }
  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    showMsg('Camera not supported on this connection. Make sure the page is loaded over HTTPS.', 'error');
    return;
  }
  try {
    if (_faceCapture) { _faceCapture.stop(); }
    _faceCapture = new FaceCapture('faceVideo', 'faceCanvas', 'faceOverlay');
    await _faceCapture.start();
    document.getElementById('faceOverlay').textContent = 'Look at the camera and blink naturally';
    document.getElementById('captureBtn').disabled = false;
    document.getElementById('captureBtn').innerHTML =
      '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-right:6px"><circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="3"/></svg>' +
      'Start Face Verification';
  } catch (e) {
    let msg = '';
    switch (e.name) {
      case 'NotAllowedError':
      case 'PermissionDeniedError':
        msg = 'Camera permission denied. Click the 🔒 icon in the address bar → Site settings → Camera → Allow, then reload.';
        break;
      case 'NotFoundError':
      case 'DevicesNotFoundError':
        msg = 'No camera found on this device. Please connect a camera and try again.';
        break;
      case 'NotReadableError':
      case 'TrackStartError':
        msg = 'Camera is in use by another app. Close it and try again.';
        break;
      default:
        msg = e.message || `Camera error (${e.name}). Please allow camera access and refresh.`;
    }
    showMsg(msg, 'error');
    console.error('[Camera]', e.name, e.message);
    // Do NOT proceed - face verification is required
  }
}

function stopCamera() {
  if (_faceCapture) { _faceCapture.stop(); _faceCapture = null; }
}

async function startCapture() {
  const btn     = document.getElementById('captureBtn');
  const overlay = document.getElementById('faceOverlay');
  btn.disabled  = true;
  btn.innerHTML = '<span class="spinner" style="width:14px;height:14px;display:inline-block;margin-right:8px;"></span>Capturing…';

  // Run client-side liveness (MediaPipe blink + head-movement).
  // The server independently re-runs liveness on the full frame sequence,
  // so bypassing the client check does not bypass verification.
  let capture;
  try {
    capture = await _faceCapture.captureAndVerify();
  } catch (e) {
    showMsg('Capture error: ' + e.message, 'error');
    btn.disabled  = false;
    btn.innerHTML = '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-right:6px"><circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="3"/></svg>Start Face Verification';
    return;
  }

  if (!capture.success) {
    showMsg(
      capture.reason === 'liveness_failed'
        ? 'Liveness check failed. Please blink or move your head slightly and try again.'
        : (capture.reason || 'Capture failed. Please try again.'),
      'error'
    );
    btn.disabled  = false;
    btn.innerHTML = '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-right:6px"><circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="3"/></svg>Start Face Verification';
    return;
  }

  overlay.textContent = 'Verifying identity…';

  // Send all frames so the server can enforce liveness independently.
  const res = await API.verifyFace(electionId, currentVoterId, capture.frames);
  stopCamera();

  btn.disabled  = false;
  btn.innerHTML = '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-right:6px"><circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="3"/></svg>Start Face Verification';

  if (!res || !res.success) {
    showMsg(res?.message || 'Face verification service error. Please try again or contact the election organiser.', 'error');
    await openCamera();
    return;
  }

  if (!res.verified) {
    const reason = res.message || '';
    const msg = reason.toLowerCase().includes('liveness')
      ? reason
      : 'Face verification failed (score: ' + (res.score || 0).toFixed(2) + '). Your face does not match. Please try again.';
    showMsg(msg, 'error');
    await openCamera();
    return;
  }

  proceedToBallot();
}

function proceedToBallot() {
  hide('stepFace');
  hideMsg();
  showVoteStep(0);
  show('stepVote');
}

/* ─────────────────────────────────────────────────────
   Step 2 - Vote per position
───────────────────────────────────────────────────── */
function showVoteStep(idx) {
  currentStep = idx;
  const pos   = positions[idx];
  const total = positions.length;

  // Progress
  const pct = Math.round((idx / total) * 100);
  document.getElementById('progressLabel').textContent = `Position ${idx + 1} of ${total}`;
  document.getElementById('progressPct').textContent   = pct + '%';
  document.getElementById('progressFill').style.width  = pct + '%';

  // Position header
  document.getElementById('posVoteNum').textContent   = idx + 1;
  document.getElementById('posVoteTitle').textContent = pos.title;
  document.getElementById('voterIdLabel').textContent = currentVoterId;

  // Back button - hide on first position
  const backBtn = document.getElementById('backBtn');
  backBtn.style.display = idx === 0 ? 'none' : 'inline-flex';

  // Next/Review button label
  const nextLabel = document.getElementById('nextBtnLabel');
  nextLabel.textContent = idx === total - 1 ? 'Review ballot' : 'Next';

  // Render candidates
  renderCandidates(pos, idx);

  // Restore previous selection for this position if navigating back
  const nextBtn = document.getElementById('nextBtn');
  nextBtn.disabled = !selections[pos.id];
}

function renderCandidates(pos, stepIdx) {
  const wrap = document.getElementById('candidateOptions');

  if (!pos.candidates || !pos.candidates.length) {
    wrap.innerHTML = '<p style="color:var(--ink-4);text-align:center;padding:16px 0;font-size:.875rem;">No candidates added for this position.</p>';
    document.getElementById('nextBtn').disabled = false;
    return;
  }

  wrap.innerHTML = pos.candidates.map((c, i) => `
    <div class="candidate-option ${selections[pos.id] === c.name ? 'selected' : ''}"
         id="opt_${stepIdx}_${i}"
         onclick="selectCandidate(${stepIdx}, ${i}, '${escJs(c.name)}')">
      <div class="radio-circle" id="rc_${stepIdx}_${i}">
        ${selections[pos.id] === c.name ? checkSvg() : ''}
      </div>
      <span class="candidate-name">${esc(c.name)}</span>
    </div>
  `).join('');
}

function selectCandidate(stepIdx, candIdx, name) {
  const pos = positions[stepIdx];
  selections[pos.id] = name;

  // Update UI
  pos.candidates.forEach((_, i) => {
    const opt = document.getElementById(`opt_${stepIdx}_${i}`);
    const rc  = document.getElementById(`rc_${stepIdx}_${i}`);
    if (!opt || !rc) return;
    opt.classList.toggle('selected', i === candIdx);
    rc.innerHTML = i === candIdx ? checkSvg() : '';
  });

  document.getElementById('nextBtn').disabled = false;
}

function goNext() {
  const pos = positions[currentStep];

  // If no candidates for this position, selections[pos.id] stays undefined - that's ok
  if (!selections[pos.id] && pos.candidates && pos.candidates.length) {
    showMsg('Please select a candidate.', 'error');
    return;
  }
  hideMsg();

  const nextIdx = currentStep + 1;
  if (nextIdx >= positions.length) {
    // Go to review
    hide('stepVote');
    showReview();
    show('stepReview');
  } else {
    showVoteStep(nextIdx);
  }
}

function goBack() {
  if (currentStep > 0) {
    hideMsg();
    showVoteStep(currentStep - 1);
  }
}

/* ─────────────────────────────────────────────────────
   Step 3 - Review & Submit
───────────────────────────────────────────────────── */
function showReview() {
  const list = document.getElementById('summaryList');
  list.innerHTML = positions.map(pos => {
    const choice = selections[pos.id];
    return `
      <div class="summary-item">
        <span class="summary-pos">${esc(pos.title)}</span>
        <span class="summary-cand">
          ${choice
            ? `<svg viewBox="0 0 24 24" width="12" height="12"><polyline points="20 6 9 17 4 12"/></svg>${esc(choice)}`
            : '<span class="summary-skip">Skipped</span>'}
        </span>
      </div>`;
  }).join('');
}

function backToVote() {
  hide('stepReview');
  show('stepVote');
  showVoteStep(positions.length - 1);
}

async function submitAllVotes() {
  const btn = document.getElementById('submitBtn');
  btn.disabled  = true;
  btn.innerHTML = '<span class="spinner" style="width:14px;height:14px;display:inline-block;margin-right:8px;"></span>Submitting…';

  // Build votes array - only include positions where a candidate was selected
  const votes = positions
    .filter(pos => selections[pos.id])
    .map(pos => ({
      position_id:    pos.id,
      candidate_name: selections[pos.id]
    }));

  const res = await API.castMultiVotes(electionId, currentVoterId, votes);

  if (res.success) {
    hide('stepReview');
    show('stepDone');
  } else {
    btn.disabled  = false;
    btn.innerHTML = 'Submit all votes <svg viewBox="0 0 24 24" width="15" height="15"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>';
    showMsg(res.message || 'Failed to submit votes. Please try again.', 'error');
  }
}

/* ─────────────────────────────────────────────────────
   Helpers
───────────────────────────────────────────────────── */
function show(id)    { const el = document.getElementById(id); if (el) el.style.display = 'block'; }
function hide(id)    { const el = document.getElementById(id); if (el) el.style.display = 'none';  }
function hideMsg()   { const el = document.getElementById('msgBox'); if (el) el.style.display = 'none'; }

function showMsg(text, type) {
  const el = document.getElementById('msgBox');
  el.textContent   = text;
  el.className     = 'msg-box ' + type;
  el.style.display = 'block';
}

function showError(text) {
  hide('loadingMsg');
  hide('stepId');
  document.getElementById('electionTitleDisplay').textContent = 'Error';
  showMsg(text, 'error');
}

function checkSvg() {
  return '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>';
}

function esc(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// Safe for use inside onclick="..." attributes (escapes single quotes)
function escJs(s) {
  return String(s).replace(/\\/g,'\\\\').replace(/'/g,"\\'");
}
