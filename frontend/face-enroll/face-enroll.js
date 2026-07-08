/**
 * face-enroll.js — Admin face enrollment
 *
 * Change 3: Embeddings generated at upload time, not at verification.
 * Change 4: Supports 3 photos (front/left/right).
 * Change 6: Photos converted to base64, sent to backend, never stored here.
 */

const params     = new URLSearchParams(location.search);
const electionId = params.get('election');
const voterId    = params.get('voter');
const photoData  = [null, null, null];

document.addEventListener('DOMContentLoaded', async () => {
  if (!Auth.requireAuth()) return;
  if (!electionId || !voterId) {
    showMsg('Missing election or voter ID in URL.', 'error');
    return;
  }
  document.getElementById('voterLabel').textContent =
    `Voter: ${decodeURIComponent(voterId)} · Election: ${electionId}`;

  // Check if face already enrolled
  await checkEnrollmentStatus();
});

async function checkEnrollmentStatus() {
  const res = await API.checkFaceEnrolled(electionId, decodeURIComponent(voterId));
  if (res && res.enrolled) {
    showAlreadyEnrolled(res.embedding_count || 1);
  }
}

/* ── Photo preview ───────────────────────────────────────────── */

function previewPhoto(idx, input) {
  const file = input.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = e => {
    photoData[idx] = e.target.result.split(',')[1];
    const preview = document.getElementById(`preview${idx}`);
    preview.innerHTML = `<img src="${e.target.result}" alt="Photo ${idx + 1}"/>`;
    preview.classList.add('filled');
    document.getElementById('enrollBtn').disabled = !photoData[0];
  };
  reader.readAsDataURL(file);
}

/* ── Enroll ──────────────────────────────────────────────────── */

async function enrollFace() {
  const photos = photoData.filter(Boolean);
  if (!photos.length) {
    showMsg('Please upload at least one photo (front-facing).', 'error');
    return;
  }

  const btn = document.getElementById('enrollBtn');
  btn.disabled  = true;
  btn.innerHTML = '<span class="btn-spinner"></span> Generating embeddings…';

  const res = await API.enrollFace(electionId, decodeURIComponent(voterId), photos);

  btn.disabled  = false;
  btn.innerHTML =
    'Generate &amp; Save Embeddings ' +
    '<svg viewBox="0 0 24 24"><line x1="5" y1="12" x2="19" y2="12"/>' +
    '<polyline points="12 5 19 12 12 19"/></svg>';

  if (res.success) {
    document.getElementById('stepUpload').style.display = 'none';
    document.getElementById('stepDone').style.display   = 'block';
    document.getElementById('doneMsg').textContent =
      res.message || 'Embeddings saved. Photos were not stored.';
    // After enroll, also show the re-enroll option
    showAlreadyEnrolled(photoData.filter(Boolean).length);
  } else {
    showMsg(res.message || 'Failed to enroll face. Try again.', 'error');
  }
}

/* ── Helpers ─────────────────────────────────────────────────── */

function showMsg(text, type) {
  const el = document.getElementById('msgBox');
  el.textContent   = text;
  el.className     = 'enroll-msg ' + type;
  el.style.display = 'block';
}

function showAlreadyEnrolled(count) {
  document.getElementById('stepUpload').style.display  = 'none';
  document.getElementById('stepDone').style.display    = 'none';
  document.getElementById('stepEnrolled').style.display = 'block';
  document.getElementById('enrolledCount').textContent =
    count + ' embedding' + (count !== 1 ? 's' : '') + ' stored';
}

function reEnroll() {
  document.getElementById('stepEnrolled').style.display = 'none';
  document.getElementById('stepUpload').style.display   = 'block';
  // Reset photo slots
  photoData.fill(null);
  for (let i = 0; i < 3; i++) {
    const p = document.getElementById(`preview${i}`);
    if (p) {
      p.classList.remove('filled');
      p.innerHTML = `<svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="4"/><path d="M4 20c0-4 3.6-7 8-7s8 3 8 7"/></svg><span>${['Front','Left','Right'][i]}</span>`;
    }
  }
  document.getElementById('enrollBtn').disabled = true;
}
