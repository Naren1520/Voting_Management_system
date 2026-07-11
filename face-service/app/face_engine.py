"""
Face engine - InsightFace wrapper.
Singleton pattern: model loads once at startup.

Change 3: Embeddings generated at enrollment, not at verification time.
Change 4: Returns one embedding per photo for multi-angle storage.
"""

import base64
import logging
import numpy as np
import cv2
from typing import List, Optional
import insightface
from insightface.app import FaceAnalysis

from app.config import settings

logger = logging.getLogger(__name__)


class FaceEngine:
    _instance: Optional["FaceEngine"] = None

    def __init__(self):
        logger.info(f"Loading InsightFace model: {settings.INSIGHTFACE_MODEL}")
        self._app = FaceAnalysis(
            name=settings.INSIGHTFACE_MODEL,
            allowed_modules=["detection", "recognition"],
        )
        # ctx_id=0 → GPU, ctx_id=-1 → CPU
        self._app.prepare(ctx_id=-1, det_size=(640, 640))
        logger.info("InsightFace model ready")

    @classmethod
    def instance(cls) -> "FaceEngine":
        if cls._instance is None:
            cls._instance = FaceEngine()
        return cls._instance

    #  Decode 

    def decode_image(self, b64: str) -> np.ndarray:
        """Decode base64 string → OpenCV BGR image."""
        # Strip data URI prefix if present (data:image/jpeg;base64,...)
        if "," in b64:
            b64 = b64.split(",", 1)[1]
        raw = base64.b64decode(b64)
        arr = np.frombuffer(raw, dtype=np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
        if img is None:
            raise ValueError("Failed to decode image - invalid base64 or format")
        return img

    #  Extract embedding 

    def get_embedding(self, img: np.ndarray) -> Optional[List[float]]:
        """
        Run InsightFace detection + recognition on one frame.
        Returns the embedding of the largest detected face, or None.
        """
        faces = self._app.get(img)
        if not faces:
            return None

        # Pick the largest face by bounding box area
        best = max(faces, key=lambda f: (f.bbox[2]-f.bbox[0]) * (f.bbox[3]-f.bbox[1]))
        emb = best.normed_embedding  # already L2-normalized by InsightFace
        return emb.tolist()

    #  Change 4: multi-photo enrollment 

    def generate_embeddings(self, b64_photos: List[str]) -> List[List[float]]:
        """
        Accepts 1–3 base64 photos (front/left/right).
        Returns one embedding per photo that contains a detectable face.
        Raises ValueError if no faces found in any photo.
        """
        embeddings = []
        for i, b64 in enumerate(b64_photos):
            try:
                img = self.decode_image(b64)
                emb = self.get_embedding(img)
                if emb is not None:
                    embeddings.append(emb)
                else:
                    logger.warning(f"No face detected in photo {i+1}")
            except Exception as e:
                logger.warning(f"Failed to process photo {i+1}: {e}")

        if not embeddings:
            raise ValueError("No faces detected in any of the provided photos")

        return embeddings

    # ── Change 5: best frame from liveness sequence ──────────────────────────

    def get_best_embedding(self, b64_frame: str) -> Optional[List[float]]:
        """
        Extracts embedding from the best frame sent by the browser
        (browser sends the sharpest/most frontal frame from its 20-30 capture sequence).
        """
        img = self.decode_image(b64_frame)
        return self.get_embedding(img)

    # ── Cosine similarity ────────────────────────────────────────────────────

    def cosine_similarity(
        self,
        live_emb: List[float],
        stored_embeddings: List[List[float]]
    ) -> float:
        """
        Change 4: Compare live embedding against ALL stored embeddings.
        Return the HIGHEST similarity score across all stored embeddings.
        InsightFace returns L2-normalized embeddings so dot product = cosine similarity.
        """
        live = np.array(live_emb, dtype=np.float32)
        # Normalize live embedding (safety - InsightFace should already do this)
        norm = np.linalg.norm(live)
        if norm > 0:
            live = live / norm

        best_score = 0.0
        for stored in stored_embeddings:
            s = np.array(stored, dtype=np.float32)
            s_norm = np.linalg.norm(s)
            if s_norm > 0:
                s = s / s_norm
            score = float(np.dot(live, s))
            if score > best_score:
                best_score = score

        return best_score
