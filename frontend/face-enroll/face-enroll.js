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

document.addEventListener('DOMContentLoaded', () => {
  if (!Auth.requireAuth()) return;
  if (!electionId || !voterId) {
    showMsg('Missing election or voter ID in URL.', 'error');
    return;
  }
  document.getElementById('voterLabel').textContent =
    `Voter: ${decodeURIComponent(voterId)} · Election: ${electionId}`;
});

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
