"""
Request authentication.
The C++ backend signs every request with FACE_API_SECRET.
This service validates the secret before processing anything.
"""

from fastapi import Header, HTTPException, status
from app.config import settings


async def verify_api_secret(authorization: str = Header(...)) -> None:
    """
    Dependency - inject into any route that should only be called by the
    C++ backend. Rejects all other callers.

    Expected header:  Authorization: Bearer <FACE_API_SECRET>
    """
    if not authorization.startswith("Bearer "):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Missing Bearer token",
        )
    token = authorization[len("Bearer "):]
    if token != settings.API_SECRET:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid API secret",
        )
