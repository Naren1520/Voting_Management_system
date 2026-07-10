/**
 * face-enroll.js
 * Supports two enrollment methods:
 *   1. Upload Photos — file input (existing flow)
 *   2. Use Camera   — live webcam capture per angle
 */

const params     = new URLSearchParams(location.search);
const electionId = params.get('election');
const voterId    = params.get('voter');

// Shared photo data — indexed 0=front, 1=left, 2=right
const photoData = [null, null, null];

// Current active tab
let activeTab = 'upload';

// Camera stream for enrollment
let enrollStream = null;

/* ─────────────────────────────────────────────────────
   Init
───────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', async () => {
  if (!Auth.requireAuth()) return;
  if (!electionId || !voterId) {
    showMsg('Missing election or voter ID in URL.', 'error');
    return;
  }
  document.getElementById('voterLabel').textContent =
    `Voter: ${decodeURIComponent(voterId)} · Election: ${electionId}`;

  await checkEnrollmentStatus();
});

async function checkEnrollmentStatus() {
  const res = await API.checkFaceEnrolled(electionId, decodeURIComponent(voterId));
  if (res && res.enrolled) {
    const banner = document.getElementById('enrolledBanner');
    banner.style.display = 'flex';
    document.getElementById('enrolledCountText').textContent =
      `${res.embedding_count || 1} embedding${res.embedding_count !== 1 ? 's' : ''} stored`;
  }
}

function dismissBanner() {
  document.getElementById('enrolledBanner').style.display = 'none';
}

/* ─────────────────────────────────────────────────────
   Tab switching
───────────────────────────────────────────────────── */
function switchTab(tab) {
  activeTab = tab;
  document.getElementById('tabUpload').classList.toggle('active', tab === 'upload');
  document.getElementById('tabCamera').classList.toggle('active', tab === 'camera');
  document.getElementById('paneUpload').style.display = tab === 'upload' ? 'block' : 'none';
  document.getElementById('paneCamera').style.display = tab === 'camera' ? 'block' : 'none';

  if (tab === 'upload') {
    closeEnrollCamera();
    removeCameraNotice();
  }
  updateEnrollBtn();
}

/* ─────────────────────────────────────────────────────
   Upload tab — file input
───────────────────────────────────────────────────── */
function handleUpload(idx, input) {
  const file = input.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = e => {
    photoData[idx] = e.target.result.split(',')[1];
    const preview = document.getElementById(`preview${idx}`);
    preview.innerHTML = `<img src="${e.target.result}" alt="Photo ${idx + 1}"/>`;
    preview.classList.add('filled');
    updateEnrollBtn();
  };
  reader.readAsDataURL(file);
}

/* ─────────────────────────────────────────────────────
   Camera tab
───────────────────────────────────────────────────── */
async function openEnrollCamera() {
  // Chrome requires HTTPS (or localhost) for camera access
  if (!window.isSecureContext) {
    showMsg(
      'Camera requires a secure connection (HTTPS). ' +
      'If you\'re testing locally, use http://localhost — not an IP address or http://.',
      'error'
    );
    return;
  }

  // mediaDevices is undefined on HTTP in Chrome
  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    showMsg(
      'Your browser does not support camera access on this connection. ' +
      'Make sure the page is loaded over HTTPS.',
      'error'
    );
    return;
  }

  try {
    enrollStream = await navigator.mediaDevices.getUserMedia({
      video: { width: { ideal: 1280 }, height: { ideal: 720 }, facingMode: 'user' },
      audio: false,
    });
    const video = document.getElementById('enrollVideo');
    video.srcObject = enrollStream;
    document.getElementById('cameraGuide').style.display = 'none';
    document.getElementById('openCamBtn').style.display  = 'none';
    document.getElementById('closeCamBtn').style.display = 'inline-flex';
    document.getElementById('angleRow').style.display    = 'flex';
    document.getElementById('angleTip').style.display    = 'flex';
    setAngleTip(0);
    removeCameraNotice(); // camera opened fine — remove any previous notice
  } catch (e) {
    let msg = '';
    const isPermissionDenied = e.name === 'NotAllowedError' || e.name === 'PermissionDeniedError';

    switch (e.name) {
      case 'NotAllowedError':
      case 'PermissionDeniedError':
        msg = 'Camera permission was denied.';
        break;

      case 'NotFoundError':
      case 'DevicesNotFoundError':
        msg = 'No camera found on this device. Please connect a camera and try again.';
        break;

      case 'NotReadableError':
      case 'TrackStartError':
        msg =
          'Camera is already in use by another app or browser tab. ' +
          'Close any apps using the camera, then try again.';
        break;

      case 'OverconstrainedError':
      case 'ConstraintNotSatisfiedError':
        // Retry with minimal constraints
        try {
          enrollStream = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
          const video = document.getElementById('enrollVideo');
          video.srcObject = enrollStream;
          document.getElementById('cameraGuide').style.display = 'none';
          document.getElementById('openCamBtn').style.display  = 'none';
          document.getElementById('closeCamBtn').style.display = 'inline-flex';
          document.getElementById('angleRow').style.display    = 'flex';
          document.getElementById('angleTip').style.display    = 'flex';
          setAngleTip(0);
          return;
        } catch (e2) {
          msg = 'Camera could not be started. Please try the Upload Photos tab instead.';
        }
        break;

      case 'SecurityError':
        msg = 'Camera access blocked by browser security policy. Make sure the page is served over HTTPS.';
        break;

      case 'AbortError':
        msg = 'Camera access was interrupted. Please try again.';
        break;

      default:
        msg = `Camera error: ${e.message || e.name}. Please try the Upload Photos tab instead.`;
    }

    showMsg(msg, 'error');

    // Show the fix-it notice only when permission was actually denied
    if (isPermissionDenied) {
      showCameraNotice();
    }

    console.error('[Camera]', e.name, e.message);
  }
}

/* Inject a camera fix-it notice below the hint — only on permission denial */
function showCameraNotice() {
  if (document.getElementById('cameraNotice')) return; // already shown
  const notice = document.createElement('div');
  notice.id        = 'cameraNotice';
  notice.className = 'camera-notice';
  notice.innerHTML =
    '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="width:15px;height:15px;flex-shrink:0">' +
      '<circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/>' +
    '</svg>' +
    '<span>To fix: click the <strong>🔒</strong> icon in the address bar → ' +
    '<strong>Site settings → Camera → Allow</strong>, then reload the page.</span>';

  // Insert it right after the enroll-hint paragraph inside paneCamera
  const hint = document.getElementById('cameraHint');
  if (hint && hint.parentNode) {
    hint.parentNode.insertBefore(notice, hint.nextSibling);
  }
}

function removeCameraNotice() {
  const notice = document.getElementById('cameraNotice');
  if (notice) notice.remove();
}

function closeEnrollCamera() {
  if (enrollStream) {
    enrollStream.getTracks().forEach(t => t.stop());
    enrollStream = null;
  }
  const video = document.getElementById('enrollVideo');
  if (video) video.srcObject = null;
  const guide = document.getElementById('cameraGuide');
  if (guide) guide.style.display = 'flex';
  const openBtn  = document.getElementById('openCamBtn');
  const closeBtn = document.getElementById('closeCamBtn');
  const angleRow = document.getElementById('angleRow');
  const angleTip = document.getElementById('angleTip');
  if (openBtn)  openBtn.style.display  = 'inline-flex';
  if (closeBtn) closeBtn.style.display = 'none';
  if (angleRow) angleRow.style.display = 'none';
  if (angleTip) angleTip.style.display = 'none';
}

function setAngleTip(idx) {
  const tips = [
    'Look straight at the camera and click Front',
    'Turn your head slightly LEFT and click Left',
    'Turn your head slightly RIGHT and click Right',
  ];
  const el = document.getElementById('angleTipText');
  if (el) el.textContent = tips[idx] || 'All angles captured!';
}

function captureAngle(idx) {
  if (!enrollStream) return;
  const video  = document.getElementById('enrollVideo');
  const canvas = document.getElementById('enrollCanvas');
  canvas.width  = video.videoWidth  || 640;
  canvas.height = video.videoHeight || 480;
  const ctx = canvas.getContext('2d');
  ctx.drawImage(video, 0, 0);
  const b64 = canvas.toDataURL('image/jpeg', 0.92).split(',')[1];
  photoData[idx] = b64;

  // Show thumbnail on angle button
  const thumb = document.getElementById(`thumb${idx}`);
  thumb.innerHTML = `<img src="data:image/jpeg;base64,${b64}" alt="angle ${idx}"/>`;
  const check = document.getElementById(`check${idx}`);
  check.style.display = 'flex';
  const btn = document.getElementById(['capFront','capLeft','capRight'][idx]);
  btn.classList.add('captured');

  // Move tip to next uncaptured angle
  const next = [0,1,2].find(i => !photoData[i]);
  if (next !== undefined) setAngleTip(next);
  else document.getElementById('angleTipText').textContent = 'All 3 angles captured! Click "Generate & Save Embeddings".';

  updateEnrollBtn();
}

/* ─────────────────────────────────────────────────────
   Shared enroll action
───────────────────────────────────────────────────── */
function updateEnrollBtn() {
  const hasAny = photoData.some(Boolean);
  document.getElementById('enrollBtn').disabled = !hasAny;
}

async function enrollFace() {
  const photos = photoData.filter(Boolean);
  if (!photos.length) {
    showMsg('Please capture or upload at least one photo.', 'error');
    return;
  }

  // Stop camera if open
  closeEnrollCamera();

  const btn = document.getElementById('enrollBtn');
  btn.disabled  = true;
  btn.innerHTML = '<span class="btn-spinner"></span> Generating embeddings…';

  const res = await API.enrollFace(electionId, decodeURIComponent(voterId), photos);

  btn.disabled  = false;
  btn.innerHTML =
    'Generate &amp; Save Embeddings ' +
    '<svg viewBox="0 0 24 24"><line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/></svg>';

  if (res.success) {
    document.getElementById('stepUpload').style.display = 'none';
    document.getElementById('stepDone').style.display   = 'block';
    document.getElementById('doneMsg').textContent =
      res.message || `${photos.length} embedding(s) saved. Photos were not stored.`;
    // Update banner
    document.getElementById('enrolledBanner').style.display = 'flex';
    document.getElementById('enrolledCountText').textContent =
      `${photos.length} embedding${photos.length !== 1 ? 's' : ''} stored`;
  } else {
    showMsg(res.message || 'Failed to enroll face. Try again.', 'error');
  }
}

function resetForm() {
  // Clear all photo data
  photoData.fill(null);

  // Reset upload previews
  for (let i = 0; i < 3; i++) {
    const p = document.getElementById(`preview${i}`);
    if (p) {
      p.classList.remove('filled');
      p.innerHTML =
        `<svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="4"/><path d="M4 20c0-4 3.6-7 8-7s8 3 8 7"/></svg>` +
        `<span>${['Front','Left','Right'][i]}</span>`;
    }
    const fileInput = document.getElementById(`photo${i}`);
    if (fileInput) fileInput.value = '';
    // Reset camera angle buttons
    const thumb = document.getElementById(`thumb${i}`);
    if (thumb) {
      thumb.innerHTML = '<svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="4"/><path d="M4 20c0-4 3.6-7 8-7s8 3 8 7"/></svg>';
    }
    const check = document.getElementById(`check${i}`);
    if (check) check.style.display = 'none';
    const capBtn = document.getElementById(['capFront','capLeft','capRight'][i]);
    if (capBtn) capBtn.classList.remove('captured');
  }

  document.getElementById('stepDone').style.display   = 'none';
  document.getElementById('stepUpload').style.display = 'block';
  document.getElementById('enrollBtn').disabled = true;
  switchTab('upload');
}

/* ─────────────────────────────────────────────────────
   Helpers
───────────────────────────────────────────────────── */
function showMsg(text, type) {
  const el = document.getElementById('msgBox');
  el.textContent   = text;
  el.className     = 'enroll-msg ' + type;
  el.style.display = 'block';
}
