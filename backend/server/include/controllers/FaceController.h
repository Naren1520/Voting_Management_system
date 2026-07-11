#pragma once
#include "../../third_party/json.hpp"
#include <string>
#include <vector>

using json = nlohmann::json;

/**
 * FaceController
 *
 * Implements Change 1: C++ backend owns all DB access.
 * Python service is stateless - it never touches the database.
 *
 * Two operations:
 *   enroll()  - admin uploads photo(s) → generate embeddings → store encrypted
 *   verify()  - fetch stored embeddings → send to Python service → return result
 */
class FaceController {
public:
    /**
     * enroll - called when admin registers a voter with photos.
     * Change 3: generates embeddings at enrollment, not at verify time.
     * Change 4: accepts up to 3 photos (front/left/right).
     * Change 6: does not store raw photos - only encrypted embeddings.
     *
     * @param userId      election owner's user_id (auth check)
     * @param electionId  election UUID
     * @param voterId     voter's voter_id string
     * @param photos      list of 1–3 base64-encoded photos
     */
    json enroll(const std::string& userId,
                const std::string& electionId,
                const std::string& voterId,
                const std::vector<std::string>& photos);

    /**
     * verify - called when voter enters voter ID on ballot page.
     * Change 1: fetches embeddings from Supabase here, passes to Python.
     * Change 2: threshold from config, optional per-request override.
     * Change 5: receives best_frame already selected by browser liveness logic.
     *
     * @param electionId       election UUID
     * @param voterId          voter's voter_id string
     * @param bestFrameBase64  best frame from browser liveness sequence (base64)
     * @param threshold        optional threshold override (0 = use default)
     */
    json verify(const std::string& electionId,
                const std::string& voterId,
                const std::string& bestFrameBase64,
                float threshold = 0.0f);

private:
    // Call the Python face service
    json callFaceService(const std::string& endpoint, const json& body);

    // Encrypt/decrypt embedding before storing in DB
    std::string encryptEmbedding(const std::string& embeddingJson);
    std::string decryptEmbedding(const std::string& encrypted);
};
