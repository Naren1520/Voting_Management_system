"""
modal_app.py — VoteStack Face Service on Modal.com
Modal v1.5+ compatible.

All app code is embedded directly in this file so Modal doesn't need
to resolve local imports. The face-service/app/ modules are added to
the image via add_local_python_source.

Deploy:
    cd face-service
    modal deploy modal_app.py

URL after deploy:
    https://narensonu1520--votestack-face-service-fastapi-app.modal.run
"""

import modal

# ── Docker image ──────────────────────────────────────────────────────────────

image = (
    modal.Image.debian_slim(python_version="3.11")
    .apt_install("libglib2.0-0", "libgl1-mesa-glx", "libgomp1")
    .pip_install(
        "fastapi==0.111.0",
        "uvicorn[standard]==0.29.0",
        "pydantic==2.7.1",
        "numpy==1.26.4",
        "opencv-python-headless==4.9.0.80",
        "insightface==0.7.3",
        "onnxruntime==1.17.3",
        "mediapipe==0.10.14",
        "python-multipart==0.0.9",
    )
    # Pre-cache the InsightFace model so cold starts are fast
    .run_commands(
        'python -c "'
        "from insightface.app import FaceAnalysis; "
        "a = FaceAnalysis(name='buffalo_sc', allowed_modules=['detection','recognition']); "
        "a.prepare(ctx_id=-1, det_size=(640,640)); "
        'print(\'InsightFace buffalo_sc cached\')"'
    )
    # Copy the entire face-service source into /app inside the image
    .add_local_python_source("app", "main")
)

# ── App ───────────────────────────────────────────────────────────────────────

app = modal.App("votestack-face-service", image=image)

# ── Web endpoint ──────────────────────────────────────────────────────────────

@app.function(
    secrets=[modal.Secret.from_name("votestack-face-secrets")],
    cpu=2,
    memory=2048,
    timeout=120,
)
@modal.asgi_app()
def fastapi_app():
    from main import app as fastapi_application
    return fastapi_application
