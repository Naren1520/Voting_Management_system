"""
POST /verify

Full verification flow with server-side liveness enforcement.

Flow:
  Voter enters voter ID
    → Browser captures 20-30 frames (raw webcam, no client-side selection)
    → Browser POSTs all frames to C++ backend
    → C++ backend fetches stored embeddings from PostgreSQL
    → C++ backend calls this endpoint with frames + stored_embeddings
    → liveness.analyse_frames() validates blink / head-movement on the sequence
    → If not live  → 200 { verified: false, reason: "liveness_failed" }
    → If live      → extract embedding from best frame, compare against stored
    → Returns { verified, score, threshold_used, reason }

Liveness is enforced server-side so a printed photo or replay attack cannot
pass: the service itself decides whether the sequence is live, not the browser.
"""

import logging
from fastapi import APIRouter, Depends, HTTPException, status

from app.models import VerifyRequest, VerifyResponse
from app.face_engine import FaceEngine
from app.liveness import analyse_frames
from app.config import settings
from app.security import verify_api_secret

router = APIRouter()
logger = logging.getLogger(__name__)


@router.post(
    "/verify",
    response_model=VerifyResponse,
    dependencies=[Depends(verify_api_secret)],
)
async def verify_face(req: VerifyRequest):
    """
    Stateless face comparison with server-side liveness check.

    Receives: frames (20-30 base64 JPEG frames) + stored_embeddings (from C++ backend).
    Returns:  verified (bool), score (float), threshold_used (float), reason (str|None).
    """
    engine = FaceEngine.instance()

    # Change 2: use per-request threshold if provided, else fall back to env var
    threshold = req.threshold if req.threshold is not None else settings.FACE_THRESHOLD

    # ── Server-side liveness check ────────────────────────────────────────────
    # analyse_frames inspects the full sequence for blink (EAR drop) and head
    # movement (nose-tip displacement). A printed photo or a single static frame
    # cannot produce these signals, so spoofing requires a live video stream.
    is_live, best_frame_idx, liveness_reason = analyse_frames(req.frames)

    if not is_live:
        logger.warning(f"Liveness failed: {liveness_reason}")
        return VerifyResponse(
            verified=False,
            score=0.0,
            threshold_used=threshold,
            reason=liveness_reason or "Liveness check failed - please blink or move your head slightly",
        )

    # ── Extract embedding from the best live frame ────────────────────────────
    best_frame = req.frames[best_frame_idx]
    try:
        live_embedding = engine.get_best_embedding(best_frame)
    except Exception as e:
        logger.error(f"Failed to decode live frame: {e}")
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail="Failed to process live frame",
        )

    if live_embedding is None:
        return VerifyResponse(
            verified=False,
            score=0.0,
            threshold_used=threshold,
            reason="No face detected in the captured frames",
        )

    if not req.stored_embeddings:
        return VerifyResponse(
            verified=False,
            score=0.0,
            threshold_used=threshold,
            reason="No stored embeddings found for this voter",
        )

    # ── Change 4: compare against all stored embeddings, take highest score ──
    score = engine.cosine_similarity(live_embedding, req.stored_embeddings)

    # Change 2: compare against configurable threshold
    verified = score >= threshold

    logger.info(
        f"Verification: score={score:.4f} threshold={threshold} verified={verified}"
    )

    return VerifyResponse(
        verified=verified,
        score=round(score, 4),
        threshold_used=threshold,
        reason=None if verified else f"Score {score:.3f} below threshold {threshold}",
    )
