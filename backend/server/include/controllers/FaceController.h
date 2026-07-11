#pragma once
#include "../../third_party/json.hpp"
#include <array>
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
 *
 * Encryption: AES-256-GCM (OpenSSL EVP)
 *   - Key loaded once from EMBEDDING_ENCRYPTION_KEY env var (64 hex chars = 32 bytes)
 *   - Server aborts at startup if the key is missing or malformed
 *   - Each encrypt call uses a fresh 12-byte random IV
 *   - Ciphertext wire format (all concatenated, then base64-encoded):
 *       [12 bytes IV] [16 bytes GCM auth tag] [N bytes ciphertext]
 */
class FaceController {
public:
    /**
     * enroll - called when admin registers a voter with photos.
     * Change 3: generates embeddings at enrollment, not at verify time.
     * Change 4: accepts up to 3 photos (front/left/right).
     * Change 6: does not store raw photos - only AES-256-GCM encrypted embeddings.
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
     * Change 5: receives the full frame sequence captured by the browser.
     *           Server-side liveness.analyse_frames() validates the sequence
     *           and selects the best frame — the browser is no longer trusted
     *           to do either job.
     *
     * @param electionId  election UUID
     * @param voterId     voter's voter_id string
     * @param frames      ordered list of 20-30 base64-encoded JPEG frames
     * @param threshold   optional threshold override (0 = use default)
     */
    json verify(const std::string& electionId,
                const std::string& voterId,
                const std::vector<std::string>& frames,
                float threshold = 0.0f);

    /**
     * loadEncryptionKey - called once at server startup.
     * Reads EMBEDDING_ENCRYPTION_KEY from the environment (64 hex chars = 32 bytes).
     * Aborts the process with a fatal log if the variable is missing or invalid —
     * the server must never start without a real key.
     */
    static void loadEncryptionKey();

private:
    // Call the Python face service
    json callFaceService(const std::string& endpoint, const json& body);

    /**
     * encryptEmbedding - AES-256-GCM encrypt.
     * Returns base64( IV[12] || Tag[16] || Ciphertext[N] ).
     * Throws std::runtime_error on OpenSSL failure.
     */
    std::string encryptEmbedding(const std::string& plain);

    /**
     * decryptEmbedding - AES-256-GCM decrypt + authenticate.
     * Expects base64( IV[12] || Tag[16] || Ciphertext[N] ).
     * Throws std::runtime_error if authentication fails or data is malformed.
     */
    std::string decryptEmbedding(const std::string& b64Blob);

    // Base64 helpers (no external dependency)
    static std::string base64Encode(const unsigned char* data, size_t len);
    static std::vector<unsigned char> base64Decode(const std::string& b64);

    // 32-byte AES-256 key shared by all instances, set by loadEncryptionKey()
    static std::array<unsigned char, 32> s_key;
    static bool                          s_keyLoaded;
};
