"""
POST /generate-embedding

Change 3: Generate embeddings at enrollment time (admin upload).
Change 4: Accept up to 3 photos (front/left/right).
Change 6: Service never stores photos - only returns embeddings.
          C++ backend stores the encrypted embeddings; photos are discarded.

Flow:
  Admin uploads photo(s)
    → C++ backend sends to this endpoint
    → Returns embeddings
    → C++ backend encrypts and stores in PostgreSQL
    → Original photos are NOT stored
"""

import logging
from fastapi import APIRouter, Depends, HTTPException, status

from app.models import EmbeddingRequest, EmbeddingResponse
from app.face_engine import FaceEngine
from app.security import verify_api_secret

router = APIRouter()
logger = logging.getLogger(__name__)


@router.post(
    "/generate-embedding",
    response_model=EmbeddingResponse,
    dependencies=[Depends(verify_api_secret)],
)
async def generate_embedding(req: EmbeddingRequest):
    """
    Accepts 1–3 base64-encoded photos.
    Returns one 512-dim embedding per photo that contains a detectable face.
    The C++ backend is responsible for encrypting and storing these embeddings.
    Original photos should be deleted after this call returns.
    """
    engine = FaceEngine.instance()

    try:
        embeddings = engine.generate_embeddings(req.photos)
    except ValueError as e:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail=str(e),
        )
    except Exception as e:
        logger.error(f"Embedding generation failed: {e}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to generate embeddings",
        )

    return EmbeddingResponse(
        success=True,
        embeddings=embeddings,
        count=len(embeddings),
    )
