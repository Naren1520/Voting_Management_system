"""
POST /verify

Full verification flow (all 6 improvements applied).

Change 1: C++ backend fetches embedding from DB — service is stateless.
Change 2: Configurable threshold via env var or per-request override.
Change 4: Compare against all stored embeddings, take best score.
Change 5: Browser sends best frame from 20-30 frame liveness sequence.
          (Liveness is handled on the browser side with MediaPipe JS;
           this endpoint receives the pre-selected best frame.)

Flow:
  Voter enters voter ID
    → Browser captures 20-30 frames (MediaPipe liveness in JS)
    → Browser selects best frame, sends to C++ backend
    → C++ backend fetches stored embeddings from PostgreSQL
    → C++ backend calls this endpoint with best_frame + stored_embeddings
    → Returns { verified, score, threshold_used }
"""

import logging
from fastapi import APIRouter, Depends, HTTPException, status

from app.models import VerifyRequest, VerifyResponse
from app.face_engine import FaceEngine
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
    Stateless face comparison.
    Receives: best_frame (base64) + stored_embeddings (from C++ backend).
    Returns:  verified (bool), score (float), threshold_used (float).
    """
    engine = FaceEngine.instance()

    # Change 2: use per-request threshold if provided, else fall back to env var
    threshold = req.threshold if req.threshold is not None else settings.FACE_THRESHOLD

    # Extract embedding from the live best frame
    try:
        live_embedding = engine.get_best_embedding(req.best_frame)
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
            reason="No face detected in the live frame",
        )

    if not req.stored_embeddings:
        return VerifyResponse(
            verified=False,
            score=0.0,
            threshold_used=threshold,
            reason="No stored embeddings found for this voter",
        )

    # Change 4: compare against all stored embeddings, take highest score
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
