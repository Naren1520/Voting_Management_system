"""
Configuration — all values from environment variables.
Change 2: Configurable threshold (no hardcoded 0.8).
"""

import os
from typing import List


class Settings:
    # Server
    PORT: int = int(os.getenv("PORT", "8000"))

    # Security — shared secret between C++ backend and this service
    # C++ backend sends this in the Authorization header
    API_SECRET: str = os.getenv("FACE_API_SECRET", "change-me-in-production")

    # Face verification threshold (Change 2)
    # Adjustable without code changes. Range: 0.0–1.0 (higher = stricter)
    FACE_THRESHOLD: float = float(os.getenv("FACE_THRESHOLD", "0.82"))

    # Liveness: minimum frames required for best-frame selection (Change 5)
    LIVENESS_MIN_FRAMES: int = int(os.getenv("LIVENESS_MIN_FRAMES", "20"))

    # CORS — only the C++ backend should call this service
    ALLOWED_ORIGINS: List[str] = os.getenv(
        "FACE_ALLOWED_ORIGINS", "https://votestack-cjom.onrender.com"
    ).split(",")

    # InsightFace model name
    # "buffalo_sc" = lightweight (CPU-friendly), "buffalo_l" = more accurate (GPU)
    INSIGHTFACE_MODEL: str = os.getenv("INSIGHTFACE_MODEL", "buffalo_sc")


settings = Settings()
