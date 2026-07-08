from fastapi import APIRouter
from app.config import settings

router = APIRouter()


@router.get("/health")
async def health():
    return {
        "status": "ok",
        "threshold": settings.FACE_THRESHOLD,
        "model": settings.INSIGHTFACE_MODEL,
    }
