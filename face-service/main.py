"""
VoteStack Face Verification Service
====================================
Stateless FastAPI microservice — no database access.
The C++ backend owns all data; this service only computes.

Endpoints:
  POST /generate-embedding   — enroll: photo -> embedding
  POST /verify               — voting: live frame + stored embedding -> result
  GET  /health               — health check
"""

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
import uvicorn

from app.routes import embedding, verify, health
from app.config import settings

app = FastAPI(
    title="VoteStack Face Service",
    description="Stateless face embedding and verification microservice",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.ALLOWED_ORIGINS,
    allow_methods=["POST", "GET"],
    allow_headers=["Content-Type", "Authorization"],
)

app.include_router(health.router)
app.include_router(embedding.router)
app.include_router(verify.router)

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=settings.PORT, reload=False)
