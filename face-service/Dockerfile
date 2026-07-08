# ============================================================
# Dockerfile — VoteStack Face Service
# Python 3.11 slim + InsightFace + MediaPipe
# ============================================================

FROM python:3.11-slim

WORKDIR /app

# System deps for OpenCV and InsightFace
RUN apt-get update && apt-get install -y \
    libglib2.0-0 \
    libgl1-mesa-glx \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Install Python deps
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Pre-download InsightFace model at build time
# so first request isn't slow
RUN python -c "\
from insightface.app import FaceAnalysis; \
app = FaceAnalysis(name='buffalo_sc', allowed_modules=['detection','recognition']); \
app.prepare(ctx_id=-1, det_size=(640,640)); \
print('Model ready')"

# Copy source
COPY . .

EXPOSE 8000

CMD ["python", "main.py"]
