"""
Liveness detection - MediaPipe FaceMesh.

Change 5: Browser captures 20-30 frames, sends them all.
This module analyses the sequence for:
  - Eye blink (EAR drop below threshold)
  - Head movement (nose tip displacement across frames)

If either is detected the sequence is considered live.
Returns the index of the best frame (sharpest, most frontal).
"""

import base64
import logging
import math
from typing import List, Optional, Tuple
import numpy as np
import cv2

logger = logging.getLogger(__name__)

# MediaPipe landmark indices
LEFT_EYE  = [33, 160, 158, 133, 153, 144]   # EAR landmarks
RIGHT_EYE = [362, 385, 387, 263, 373, 380]
NOSE_TIP  = 1

EAR_BLINK_THRESHOLD  = 0.22   # below this → eye considered closed
HEAD_MOVE_THRESHOLD  = 0.02   # fraction of frame width


def _eye_aspect_ratio(landmarks, indices: List[int]) -> float:
    """Compute Eye Aspect Ratio from 6 landmark points."""
    p = [landmarks[i] for i in indices]
    # Vertical distances
    v1 = math.sqrt((p[1].x-p[5].x)**2 + (p[1].y-p[5].y)**2)
    v2 = math.sqrt((p[2].x-p[4].x)**2 + (p[2].y-p[4].y)**2)
    # Horizontal distance
    h  = math.sqrt((p[0].x-p[3].x)**2 + (p[0].y-p[3].y)**2)
    return (v1 + v2) / (2.0 * h) if h > 0 else 0.0


def _sharpness(img: np.ndarray) -> float:
    """Laplacian variance - higher = sharper."""
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    return float(cv2.Laplacian(gray, cv2.CV_64F).var())


def _decode(b64: str) -> Optional[np.ndarray]:
    try:
        if "," in b64:
            b64 = b64.split(",", 1)[1]
        raw = base64.b64decode(b64)
        arr = np.frombuffer(raw, dtype=np.uint8)
        return cv2.imdecode(arr, cv2.IMREAD_COLOR)
    except Exception:
        return None


def analyse_frames(b64_frames: List[str]) -> Tuple[bool, int, str]:
    """
    Analyse a sequence of base64-encoded frames for liveness.

    Returns:
        (is_live, best_frame_index, reason)
        reason is populated on failure.
    """
    try:
        import mediapipe as mp
        mp_face_mesh = mp.solutions.face_mesh
    except ImportError:
        logger.warning("MediaPipe not installed - liveness check skipped")
        # Graceful degradation: still pick the sharpest frame
        return _fallback_best_frame(b64_frames)

    ears: List[float] = []
    nose_positions: List[float] = []
    sharpness_scores: List[float] = []
    valid_frames: List[int] = []

    with mp_face_mesh.FaceMesh(
        static_image_mode=True,
        max_num_faces=1,
        refine_landmarks=True,
        min_detection_confidence=0.5,
    ) as mesh:
        for idx, b64 in enumerate(b64_frames):
            img = _decode(b64)
            if img is None:
                continue

            rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
            result = mesh.process(rgb)

            if not result.multi_face_landmarks:
                continue

            lm = result.multi_face_landmarks[0].landmark
            left_ear  = _eye_aspect_ratio(lm, LEFT_EYE)
            right_ear = _eye_aspect_ratio(lm, RIGHT_EYE)
            avg_ear   = (left_ear + right_ear) / 2.0

            ears.append(avg_ear)
            nose_positions.append(lm[NOSE_TIP].x)
            sharpness_scores.append(_sharpness(img))
            valid_frames.append(idx)

    if len(valid_frames) < 3:
        return False, 0, "Too few valid frames - ensure your face is visible"

    # ── Blink detection ───────────────────────────────────────────────────
    blink_detected = any(e < EAR_BLINK_THRESHOLD for e in ears)

    # ── Head movement detection ───────────────────────────────────────────
    nose_range = max(nose_positions) - min(nose_positions)
    head_move  = nose_range > HEAD_MOVE_THRESHOLD

    is_live = blink_detected or head_move

    if not is_live:
        return False, 0, "Liveness check failed - please blink or move your head slightly"

    # ── Best frame: sharpest with EAR near mean (eyes open) ──────────────
    mean_ear = sum(ears) / len(ears)
    # Rank by sharpness but penalise frames where eyes are closed
    ranked = sorted(
        range(len(valid_frames)),
        key=lambda i: sharpness_scores[i] * (1.0 if ears[i] > mean_ear * 0.8 else 0.3),
        reverse=True,
    )
    best_local_idx = ranked[0]
    best_frame_idx = valid_frames[best_local_idx]

    return True, best_frame_idx, ""


def _fallback_best_frame(b64_frames: List[str]) -> Tuple[bool, int, str]:
    """Fallback when MediaPipe is unavailable - pick sharpest frame."""
    best_idx   = 0
    best_score = -1.0
    for idx, b64 in enumerate(b64_frames):
        img = _decode(b64)
        if img is None:
            continue
        score = _sharpness(img)
        if score > best_score:
            best_score = score
            best_idx   = idx
    # Without liveness we cannot confirm it's live - still return True
    # to avoid blocking voters when MediaPipe is not installed.
    return True, best_idx, "mediapipe_unavailable"
