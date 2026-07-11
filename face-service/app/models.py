"""
Pydantic request/response models.
"""

from pydantic import BaseModel, Field
from typing import List, Optional


#  Enroll (admin uploads photo) 

class EmbeddingRequest(BaseModel):
    """
    Change 4: Accept multiple photos (front, left, right).
    C++ backend sends all three base64-encoded frames.
    """
    photos: List[str] = Field(
        ...,
        min_items=1,
        max_items=3,
        description="1–3 base64-encoded JPEG/PNG images (front, left, right)"
    )
    # Optional: override global threshold for this election
    threshold: Optional[float] = Field(
        None,
        ge=0.0, le=1.0,
        description="Per-election threshold override"
    )


class EmbeddingResponse(BaseModel):
    success: bool
    # Change 4: list of embeddings (one per photo)
    # C++ backend averages or stores all; service returns all.
    embeddings: List[List[float]]
    count: int


#  Verify (voter at ballot) 

class VerifyRequest(BaseModel):
    """
    Change 1: C++ backend fetches embedding from DB and passes it here.
    Change 5: Browser sends the full liveness frame sequence (20-30 frames).
              Server-side liveness.analyse_frames() validates the sequence and
              selects the best frame - the browser no longer does either job.
    """
    # Full frame sequence from the browser (20-30 base64 JPEG frames).
    # Server-side liveness analysis runs on this sequence.
    frames: List[str] = Field(
        ...,
        min_items=3,
        description="Ordered sequence of 20-30 base64-encoded JPEG frames captured by the browser"
    )

    # Change 1: C++ backend owns the DB; it fetches and sends embeddings here
    # Change 4: list of stored embeddings (one per enrollment photo)
    stored_embeddings: List[List[float]] = Field(
        ...,
        description="Stored embeddings fetched by C++ backend from PostgreSQL"
    )

    # Change 2: optional per-request threshold override
    threshold: Optional[float] = Field(
        None,
        ge=0.0, le=1.0,
        description="Threshold override (falls back to FACE_THRESHOLD env var)"
    )


class VerifyResponse(BaseModel):
    verified: bool
    score: float          # highest cosine similarity score across all stored embeddings
    threshold_used: float
    reason: Optional[str] = None  # populated on failure
