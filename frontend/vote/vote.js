/**
 * vote.js - Standard single-candidate election ballot
 * Matches vote-multi flow: Voter ID → Face Verify → Select → Done
 */

const params     = new URLSearchParams(location.search);
const electionId = params.get('election');

let candidates     = [];
let selectedCand   = null;
let currentVoterId = null;
let faceStream     = null;

/* ─────────────────────────────────────────────────────
   Init
───────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', async () => {
  if (!electionId) { showError('Invalid voting link.'); return; }

  const [infoRes, candRes] = await Promise.all([
    API.getElectionInfo(electionId),
    API.publicCandidates(electionId)
  ]);

  hide('loadingMsg');

  if (!infoRes.success) { showError(infoRes.message || 'Election not found.'); return; }

  document.getElementById('electionTitleDisplay').textContent = infoRes.title;
  document.title = infoRes.title + ' - VoteStack';

  if (!infoRes.is_active) { showError('This election is closed.'); return; }

  // Store face verify flag - used when routing from voter ID step
  window._faceVerifyEnabled = !!infoRes.face_verify_enabled;

  // Schedule check
  const sched = {
    schedule_type: infoRes.schedule_type || 'always_on',
    timezone:      infoRes.timezone,
    starts_at:     infoRes.starts_at,
    ends_at:       infoRes.ends_at,
    schedule_json: infoRes.schedule_json,
  };
  const status = Schedule.getStatus(sched);
  if (!status.open) {
    const block = document.getElementById('schedBlock');
    block.style.display = 'block';
    document.getElementById('schedBlockLabel').textContent  = status.label;
    document.getElementById('schedBlockReason').textContent = status.reason;
    if (status.nextChange) {
      Schedule.startCountdown(status.nextChange, document.getElementById('schedBlockCountdown'));
    }
    return;
  }

  if (!candRes.success) { showError(candRes.message || 'Failed to load candidates.'); return; }
  candidates = candRes.candidates || [];

  show('stepId');
});

/* ─────────────────────────────────────────────────────
   Step 1 - Verify Voter ID
───────────────────────────────────────────────────── */
async function checkVoter() {
  const voterId = document.getElementById('voterIdInput').value.trim();
  const btn     = document.getElementById('checkBtn');
  if (!voterId) { showMsg('Enter your voter ID.', 'error'); return; }

  btn.disabled  = true;
  btn.innerHTML = '<span class="spinner" style="width:16px;height:16px;display:inline-block;margin-right:8px;border-width:2px;"></span>Verifying…';

  const res = await API.checkVoter(electionId, voterId);

  btn.disabled  = false;
  btn.innerHTML = 'Verify &amp; Continue <svg viewBox="0 0 24 24"><line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/></svg>';

  if (res.already_voted) {
    showMsg('You have already voted in this election.', 'error');
    return;
  }
  if (!res.success) {
    showMsg(res.message || 'Voter not found.', 'error');
    return;
  }

  currentVoterId = voterId;
  document.getElementById('voterIdLabel').textContent  = voterId;
  document.getElementById('faceVoterLabel').textContent = voterId;

  hide('stepId');
  hideMsg();

  // Skip face step if admin has disabled it for this election
  if (!window._faceVerifyEnabled) {
    show('stepVote');
    renderCandidates();
    return;
  }

  show('stepFace');
  await openCamera();
}

/* ─────────────────────────────────────────────────────
   Step 2 - Face Verification
───────────────────────────────────────────────────── */
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
    faceStream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: 'user' }, audio: false });
    document.getElementById('faceVideo').srcObject = faceStream;
    document.getElementById('faceOverlay').textContent = 'Look at the camera and blink naturally';
    document.getElementById('captureBtn').disabled = false;
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
        msg = `Camera error (${e.name}): ${e.message}. Please allow camera access and refresh.`;
    }
    showMsg(msg, 'error');
    console.error('[Camera]', e.name, e.message);
  }
}

function stopCamera() {
  if (faceStream) { faceStream.getTracks().forEach(t => t.stop()); faceStream = null; }
}

async function startCapture() {
  const btn     = document.getElementById('captureBtn');
  const overlay = document.getElementById('faceOverlay');

  btn.disabled  = true;
  btn.innerHTML = '<span class="spinner" style="width:14px;height:14px;display:inline-block;margin-right:8px;border-width:2px;"></span>Capturing…';
  overlay.textContent = 'Hold still… capturing frames';

  const video  = document.getElementById('faceVideo');
  const canvas = document.getElementById('faceCanvas');
  canvas.width  = video.videoWidth  || 640;
  canvas.height = video.videoHeight || 480;

  // Capture best of 10 frames (sharpest = largest file size)
  let bestFrame = null, bestSize = 0;
  for (let i = 0; i < 10; i++) {
    const ctx = canvas.getContext('2d');
    ctx.drawImage(video, 0, 0);
    const b64 = canvas.toDataURL('image/jpeg', 0.9).split(',')[1];
    if (b64.length > bestSize) { bestSize = b64.length; bestFrame = b64; }
    await new Promise(r => setTimeout(r, 80));
  }

  overlay.textContent = 'Verifying identity…';
  const res = await API.verifyFace(electionId, currentVoterId, bestFrame);
  stopCamera();

  btn.disabled  = false;
  btn.innerHTML = '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-right:6px"><circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="3"/></svg>Start Face Verification';

  if (!res || !res.success) {
    showMsg('Face verification service error. Please try again or contact the organiser.', 'error');
    await openCamera();
    return;
  }
  if (!res.verified) {
    showMsg('Face verification failed (score: ' + (res.score || 0).toFixed(2) + '). Your face does not match. Please try again.', 'error');
    await openCamera();
    return;
  }

  // Verified - show ballot
  hide('stepFace');
  hideMsg();
  show('stepVote');
  renderCandidates();
}

/* ─────────────────────────────────────────────────────
   Step 3 - Select Candidate
───────────────────────────────────────────────────── */
function renderCandidates() {
  const wrap = document.getElementById('candidateOptions');
  if (!candidates.length) {
    wrap.innerHTML = '<p style="text-align:center;color:var(--ink-4);font-size:.875rem;padding:16px 0;">No candidates added yet.</p>';
    return;
  }
  wrap.innerHTML = candidates.map((c, i) => `
    <div class="candidate-option" id="opt_${i}" onclick="selectCand(${i}, '${escJs(c.name)}')">
      <div class="radio-circle" id="rc_${i}"></div>
      <span class="candidate-name">${esc(c.name)}</span>
    </div>
  `).join('');
}

function selectCand(idx, name) {
  candidates.forEach((_, i) => {
    const opt = document.getElementById(`opt_${i}`);
    const rc  = document.getElementById(`rc_${i}`);
    if (!opt || !rc) return;
    opt.classList.toggle('selected', i === idx);
    rc.innerHTML = i === idx
      ? '<svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="#fff" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>'
      : '';
  });
  selectedCand = name;
  document.getElementById('castBtn').disabled = false;
}

async function castVote() {
  if (!selectedCand) { showMsg('Please select a candidate.', 'error'); return; }
  const btn = document.getElementById('castBtn');
  btn.disabled  = true;
  btn.innerHTML = '<span class="spinner" style="width:14px;height:14px;display:inline-block;margin-right:8px;border-width:2px;"></span>Casting vote…';

  const res = await API.castVote(electionId, currentVoterId, selectedCand);

  if (res.success) {
    hide('stepVote');
    show('stepDone');
  } else {
    showMsg(res.message || 'Failed to cast vote. Please try again.', 'error');
    btn.disabled  = false;
    btn.innerHTML = 'Cast Vote <svg viewBox="0 0 24 24"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>';
  }
}

function backToId() {
  stopCamera();
  hide('stepFace');
  hide('stepVote');
  hideMsg();
  selectedCand = null;
  show('stepId');
}

/* ─────────────────────────────────────────────────────
   Helpers
───────────────────────────────────────────────────── */
function show(id)  { const el = document.getElementById(id); if (el) el.style.display = 'block'; }
function hide(id)  { const el = document.getElementById(id); if (el) el.style.display = 'none';  }
function hideMsg() { const el = document.getElementById('msgBox'); if (el) el.style.display = 'none'; }

function showMsg(text, type) {
  const el = document.getElementById('msgBox');
  el.textContent   = text;
  el.className     = 'msg-box ' + type;
  el.style.display = 'block';
}

function showError(text) {
  hide('loadingMsg');
  document.getElementById('electionTitleDisplay').textContent = 'Error';
  showMsg(text, 'error');
}

function esc(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function escJs(s) {
  return String(s).replace(/\\/g,'\\\\').replace(/'/g,"\\'");
}
