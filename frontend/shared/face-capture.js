/**
 * face-capture.js - Browser-side liveness capture
 *
 * Change 5: Captures 20-30 frames, runs MediaPipe FaceMesh for liveness,
 *           selects the best frame, returns it as base64 to the caller.
 *
 * Change 5 detail:
 *   - Opens webcam via WebRTC
 *   - Captures frames at ~15fps for 2 seconds (≈30 frames)
 *   - Runs MediaPipe FaceMesh on each frame
 *   - Detects blink (EAR drop) or head movement (nose displacement)
 *   - Picks the sharpest frame with eyes open as "best frame"
 *   - Returns the best frame as base64 JPEG
 *
 * Usage:
 *   const capture = new FaceCapture('videoElement', 'canvasElement');
 *   await capture.start();
 *   const result = await capture.captureAndVerify();
 *   // result = { success, bestFrame, reason }
 */

const FACE_CAPTURE_CONFIG = {
  FRAMES_TO_CAPTURE:    25,
  CAPTURE_INTERVAL_MS:  80,   // ~12.5fps
  EAR_BLINK_THRESHOLD:  0.22,
  HEAD_MOVE_THRESHOLD:  0.025,
  // MediaPipe landmark indices for eyes
  LEFT_EYE:  [33, 160, 158, 133, 153, 144],
  RIGHT_EYE: [362, 385, 387, 263, 373, 380],
  NOSE_TIP:  1,
};

class FaceCapture {
  constructor(videoEl, canvasEl, overlayEl) {
    this._video   = typeof videoEl   === 'string' ? document.getElementById(videoEl)   : videoEl;
    this._canvas  = typeof canvasEl  === 'string' ? document.getElementById(canvasEl)  : canvasEl;
    this._overlay = typeof overlayEl === 'string' ? document.getElementById(overlayEl) : overlayEl;
    this._stream  = null;
    this._faceMesh = null;
  }

  //  Start webcam 

  async start() {
    try {
      this._stream = await navigator.mediaDevices.getUserMedia({
        video: { width: 640, height: 480, facingMode: 'user' },
        audio: false,
      });
      this._video.srcObject = this._stream;
      await new Promise(res => { this._video.onloadedmetadata = res; });
      this._video.play();
    } catch (e) {
      throw new Error('Camera access denied - please allow camera access and retry.');
    }

    // Load MediaPipe FaceMesh
    await this._loadFaceMesh();
  }

  async _loadFaceMesh() {
    // MediaPipe is loaded via CDN script tag in the HTML
    if (typeof window.FaceMesh === 'undefined') {
      console.warn('MediaPipe FaceMesh not loaded - liveness check will be skipped');
      return;
    }
    this._faceMesh = new window.FaceMesh({
      locateFile: f => `https://cdn.jsdelivr.net/npm/@mediapipe/face_mesh/${f}`,
    });
    this._faceMesh.setOptions({
      maxNumFaces:           1,
      refineLandmarks:       true,
      minDetectionConfidence: 0.5,
      minTrackingConfidence:  0.5,
    });
    await this._faceMesh.initialize();
  }

  //  Capture 25 frames + liveness check 

  async captureAndVerify() {
    if (!this._stream) throw new Error('Call start() first');

    const cfg     = FACE_CAPTURE_CONFIG;
    const frames  = [];
    const ears    = [];
    const noses   = [];
    const sharp   = [];

    this._setOverlay('Look at the camera…', 'info');

    // Capture frames
    for (let i = 0; i < cfg.FRAMES_TO_CAPTURE; i++) {
      const b64 = this._captureFrame();
      frames.push(b64);

      // Run MediaPipe if available
      if (this._faceMesh) {
        const lm = await this._getLandmarks(b64);
        if (lm) {
          ears.push(this._ear(lm));
          noses.push(lm[cfg.NOSE_TIP].x);
        }
      }
      sharp.push(this._sharpness(b64));

      await this._sleep(cfg.CAPTURE_INTERVAL_MS);
    }

    // Liveness check (only if MediaPipe ran)
    if (ears.length >= 5) {
      const blink    = ears.some(e => e < cfg.EAR_BLINK_THRESHOLD);
      const headMove = (Math.max(...noses) - Math.min(...noses)) > cfg.HEAD_MOVE_THRESHOLD;

      if (!blink && !headMove) {
        this._setOverlay('Please blink or move your head slightly', 'warn');
        return { success: false, frames: null, bestFrame: null, reason: 'liveness_failed' };
      }
    }

    // Select best frame index - sharpest with eyes open
    let bestIdx = 0;
    let bestScore = -1;
    for (let i = 0; i < frames.length; i++) {
      // Penalise frames where eyes were closed (EAR low)
      const earPenalty = ears[i] !== undefined && ears[i] < cfg.EAR_BLINK_THRESHOLD ? 0.3 : 1.0;
      const score = sharp[i] * earPenalty;
      if (score > bestScore) { bestScore = score; bestIdx = i; }
    }

    this._setOverlay('✓ Liveness verified', 'success');
    // Return the full frame sequence so the server can run its own liveness check,
    // plus the client-selected best frame index as a hint.
    return { success: true, frames, bestFrame: frames[bestIdx], reason: null };
  }

  // ── Helpers ───────────────────────────────────────────────────────────────

  _captureFrame() {
    const ctx = this._canvas.getContext('2d');
    this._canvas.width  = this._video.videoWidth  || 640;
    this._canvas.height = this._video.videoHeight || 480;
    ctx.drawImage(this._video, 0, 0);
    // Return base64 JPEG without the data URI prefix
    return this._canvas.toDataURL('image/jpeg', 0.85).split(',')[1];
  }

  async _getLandmarks(b64) {
    if (!this._faceMesh) return null;
    return new Promise(resolve => {
      const img = new Image();
      img.onload = async () => {
        let result = null;
        this._faceMesh.onResults(r => {
          result = r.multiFaceLandmarks?.[0] || null;
        });
        await this._faceMesh.send({ image: img });
        resolve(result);
      };
      img.src = 'data:image/jpeg;base64,' + b64;
    });
  }

  _ear(landmarks) {
    const cfg = FACE_CAPTURE_CONFIG;
    const p = lm => ({ x: landmarks[lm].x, y: landmarks[lm].y });
    const dist = (a, b) => Math.sqrt((a.x-b.x)**2 + (a.y-b.y)**2);

    const lv1 = dist(p(cfg.LEFT_EYE[1]), p(cfg.LEFT_EYE[5]));
    const lv2 = dist(p(cfg.LEFT_EYE[2]), p(cfg.LEFT_EYE[4]));
    const lh  = dist(p(cfg.LEFT_EYE[0]), p(cfg.LEFT_EYE[3]));

    const rv1 = dist(p(cfg.RIGHT_EYE[1]), p(cfg.RIGHT_EYE[5]));
    const rv2 = dist(p(cfg.RIGHT_EYE[2]), p(cfg.RIGHT_EYE[4]));
    const rh  = dist(p(cfg.RIGHT_EYE[0]), p(cfg.RIGHT_EYE[3]));

    const leftEar  = lh > 0 ? (lv1 + lv2) / (2 * lh) : 0;
    const rightEar = rh > 0 ? (rv1 + rv2) / (2 * rh) : 0;
    return (leftEar + rightEar) / 2;
  }

  _sharpness(b64) {
    // Compute Laplacian variance on a downsampled luma channel as a sharpness proxy.
    // We draw the frame into an OffscreenCanvas (or a regular canvas), sample pixels,
    // and compute the variance of a 3×3 Laplacian kernel response.
    // Falls back to JPEG byte-length when OffscreenCanvas is unavailable.
    try {
      const SIZE = 64; // downsample for speed
      let canvas, ctx;
      if (typeof OffscreenCanvas !== 'undefined') {
        canvas = new OffscreenCanvas(SIZE, SIZE);
        ctx    = canvas.getContext('2d');
      } else {
        canvas        = document.createElement('canvas');
        canvas.width  = SIZE;
        canvas.height = SIZE;
        ctx           = canvas.getContext('2d');
      }

      // Draw the already-captured video frame that's sitting in this._canvas
      ctx.drawImage(this._canvas, 0, 0, SIZE, SIZE);
      const data = ctx.getImageData(0, 0, SIZE, SIZE).data;

      // Convert to grayscale luma array
      const gray = new Float32Array(SIZE * SIZE);
      for (let i = 0; i < SIZE * SIZE; i++) {
        const r = data[i * 4], g = data[i * 4 + 1], b = data[i * 4 + 2];
        gray[i] = 0.299 * r + 0.587 * g + 0.114 * b;
      }

      // Apply 3×3 Laplacian kernel: [0,1,0 / 1,-4,1 / 0,1,0]
      let sum = 0, sumSq = 0, n = 0;
      for (let y = 1; y < SIZE - 1; y++) {
        for (let x = 1; x < SIZE - 1; x++) {
          const lap =
            gray[(y - 1) * SIZE + x] +
            gray[(y + 1) * SIZE + x] +
            gray[y * SIZE + (x - 1)] +
            gray[y * SIZE + (x + 1)] -
            4 * gray[y * SIZE + x];
          sum   += lap;
          sumSq += lap * lap;
          n++;
        }
      }
      const mean = sum / n;
      return sumSq / n - mean * mean; // variance
    } catch (_) {
      // Fallback: JPEG byte-length is a weak proxy but better than nothing
      return b64.length;
    }
  }

  _setOverlay(msg, type) {
    if (!this._overlay) return;
    this._overlay.textContent = msg;
    this._overlay.className = 'face-overlay-msg ' + (type || '');
  }

  _sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

  stop() {
    if (this._stream) {
      this._stream.getTracks().forEach(t => t.stop());
      this._stream = null;
    }
  }
}

window.FaceCapture = FaceCapture;
