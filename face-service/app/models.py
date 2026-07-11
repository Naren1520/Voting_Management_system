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
    Change 5: C++ backend sends best frame (selected from 20–30 captured frames).
    """
    # Live capture - best frame selected from liveness sequence (Change 5)
    best_frame: str = Field(..., description="Base64-encoded best frame from liveness sequence")

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
