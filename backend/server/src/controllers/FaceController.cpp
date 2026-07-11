#include "../../include/controllers/FaceController.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"
#include "../../include/core/Config.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <curl/curl.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// libcurl write callback
// ─────────────────────────────────────────────────────────────────────────────

static size_t faceWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = reinterpret_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// ─────────────────────────────────────────────────────────────────────────────
// callFaceService - direct libcurl call to Modal (or any external URL)
// Change 1: Python service never calls Supabase.
// ─────────────────────────────────────────────────────────────────────────────

json FaceController::callFaceService(const std::string& endpoint, const json& body) {
    const char* faceUrl = std::getenv("FACE_SERVICE_URL");
    const char* faceKey = std::getenv("FACE_API_SECRET");

    if (!faceUrl || !faceKey) {
        LOG_WARN("[FaceController] FACE_SERVICE_URL or FACE_API_SECRET not set");
        json err; err["success"] = false;
        err["message"] = "Face service not configured (set FACE_SERVICE_URL and FACE_API_SECRET)";
        return err;
    }

    std::string url = std::string(faceUrl) + endpoint;
    std::string bodyStr = body.dump();
    std::string authHeader = "Authorization: Bearer " + std::string(faceKey);
    std::string responseBody;

    CURL* curl = curl_easy_init();
    if (!curl) {
        json err; err["success"] = false;
        err["message"] = "Failed to init HTTP client";
        return err;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST,           1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  static_cast<long>(bodyStr.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  faceWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        90L);   // 90s - Modal cold start
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("[FaceController] curl failed: " + std::string(curl_easy_strerror(res)));
        json err; err["success"] = false;
        err["message"] = "Failed to reach face service";
        return err;
    }

    LOG_INFO("[FaceController] " + endpoint + " -> HTTP " + std::to_string(httpCode));

    try {
        return json::parse(responseBody);
    } catch (...) {
        LOG_ERROR("[FaceController] Invalid JSON from face service: " + responseBody.substr(0, 200));
        json err; err["success"] = false;
        err["message"] = "Invalid response from face service";
        return err;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Static key storage
// ─────────────────────────────────────────────────────────────────────────────

std::array<unsigned char, 32> FaceController::s_key      = {};
bool                          FaceController::s_keyLoaded = false;

// ─────────────────────────────────────────────────────────────────────────────
// loadEncryptionKey
// Call once at startup. Reads EMBEDDING_ENCRYPTION_KEY (64 hex chars = 32 bytes).
// Aborts if missing or malformed — the server must never run without a real key.
// ─────────────────────────────────────────────────────────────────────────────

void FaceController::loadEncryptionKey() {
    const char* hexKey = std::getenv("EMBEDDING_ENCRYPTION_KEY");
    if (!hexKey || std::strlen(hexKey) == 0) {
        LOG_ERROR("[FaceController] FATAL: EMBEDDING_ENCRYPTION_KEY env var is not set. "
                  "Generate one with: openssl rand -hex 32");
        std::abort();
    }
    if (std::strlen(hexKey) != 64) {
        LOG_ERROR("[FaceController] FATAL: EMBEDDING_ENCRYPTION_KEY must be exactly "
                  "64 hex characters (32 bytes). Got " +
                  std::to_string(std::strlen(hexKey)) + " chars.");
        std::abort();
    }
    for (int i = 0; i < 32; ++i) {
        char byte[3] = { hexKey[i*2], hexKey[i*2+1], '\0' };
        char* end = nullptr;
        unsigned long val = std::strtoul(byte, &end, 16);
        if (end != byte + 2) {
            LOG_ERROR("[FaceController] FATAL: EMBEDDING_ENCRYPTION_KEY contains "
                      "invalid hex characters.");
            std::abort();
        }
        s_key[i] = static_cast<unsigned char>(val);
    }
    s_keyLoaded = true;
    LOG_INFO("[FaceController] AES-256-GCM encryption key loaded successfully.");
}

// Base64 helpers (RFC 4648, no line breaks)

static const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string FaceController::base64Encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int b = (static_cast<unsigned int>(data[i]) << 16);
        if (i+1 < len) b |= (static_cast<unsigned int>(data[i+1]) << 8);
        if (i+2 < len) b |= static_cast<unsigned int>(data[i+2]);
        out += kB64Chars[(b >> 18) & 0x3F];
        out += kB64Chars[(b >> 12) & 0x3F];
        out += (i+1 < len) ? kB64Chars[(b >> 6) & 0x3F] : '=';
        out += (i+2 < len) ? kB64Chars[ b       & 0x3F] : '=';
    }
    return out;
}

std::vector<unsigned char> FaceController::base64Decode(const std::string& b64) {
    // Build a reverse lookup table
    static unsigned char table[256];
    static bool tableBuilt = false;
    if (!tableBuilt) {
        std::memset(table, 0xFF, sizeof(table));
        for (int i = 0; i < 64; ++i)
            table[static_cast<unsigned char>(kB64Chars[i])] = static_cast<unsigned char>(i);
        table[static_cast<unsigned char>('=')] = 0;
        tableBuilt = true;
    }

    std::vector<unsigned char> out;
    out.reserve((b64.size() / 4) * 3);
    for (size_t i = 0; i < b64.size(); i += 4) {
        if (i + 3 >= b64.size() && b64.size() % 4 != 0)
            throw std::runtime_error("base64Decode: invalid input length");
        unsigned char a = table[static_cast<unsigned char>(b64[i])];
        unsigned char b = table[static_cast<unsigned char>(b64[i+1])];
        unsigned char c = table[static_cast<unsigned char>(b64[i+2])];
        unsigned char d = table[static_cast<unsigned char>(b64[i+3])];
        if (a == 0xFF || b == 0xFF || c == 0xFF || d == 0xFF)
            throw std::runtime_error("base64Decode: invalid character");
        out.push_back(static_cast<unsigned char>((a << 2) | (b >> 4)));
        if (b64[i+2] != '=') out.push_back(static_cast<unsigned char>((b << 4) | (c >> 2)));
        if (b64[i+3] != '=') out.push_back(static_cast<unsigned char>((c << 6) | d));
    }
    return out;
}

// encryptEmbedding - AES-256-GCM
// Wire format (before base64): [IV: 12 bytes][Tag: 16 bytes][Ciphertext: N bytes]

std::string FaceController::encryptEmbedding(const std::string& plain) {
    if (!s_keyLoaded)
        throw std::runtime_error("encryptEmbedding: encryption key not loaded");

    constexpr int IV_LEN  = 12;  // 96-bit IV recommended for GCM
    constexpr int TAG_LEN = 16;  // 128-bit authentication tag

    // Generate a cryptographically random IV for each encryption
    unsigned char iv[IV_LEN];
    if (RAND_bytes(iv, IV_LEN) != 1)
        throw std::runtime_error("encryptEmbedding: failed to generate random IV");

    const auto* plainData = reinterpret_cast<const unsigned char*>(plain.data());
    const int   plainLen  = static_cast<int>(plain.size());

    std::vector<unsigned char> ciphertext(plainLen + EVP_MAX_BLOCK_LENGTH);
    int cipherLen = 0;
    int finalLen  = 0;
    unsigned char tag[TAG_LEN];

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("encryptEmbedding: EVP_CIPHER_CTX_new failed");

    auto cleanup = [&]() { EVP_CIPHER_CTX_free(ctx); };

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        cleanup(); throw std::runtime_error("encryptEmbedding: EVP_EncryptInit_ex (cipher) failed");
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, nullptr) != 1) {
        cleanup(); throw std::runtime_error("encryptEmbedding: set IV length failed");
    }
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, s_key.data(), iv) != 1) {
        cleanup(); throw std::runtime_error("encryptEmbedding: EVP_EncryptInit_ex (key/iv) failed");
    }
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &cipherLen, plainData, plainLen) != 1) {
        cleanup(); throw std::runtime_error("encryptEmbedding: EVP_EncryptUpdate failed");
    }
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + cipherLen, &finalLen) != 1) {
        cleanup(); throw std::runtime_error("encryptEmbedding: EVP_EncryptFinal_ex failed");
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag) != 1) {
        cleanup(); throw std::runtime_error("encryptEmbedding: failed to get GCM tag");
    }
    cleanup();

    const int totalCipherLen = cipherLen + finalLen;

    // Pack: IV || Tag || Ciphertext
    std::vector<unsigned char> blob;
    blob.reserve(IV_LEN + TAG_LEN + totalCipherLen);
    blob.insert(blob.end(), iv,                        iv + IV_LEN);
    blob.insert(blob.end(), tag,                       tag + TAG_LEN);
    blob.insert(blob.end(), ciphertext.begin(),        ciphertext.begin() + totalCipherLen);

    return base64Encode(blob.data(), blob.size());
}

// decryptEmbedding - AES-256-GCM
// Expects base64( IV[12] || Tag[16] || Ciphertext[N] )
// Throws if authentication fails (data was tampered or wrong key).

std::string FaceController::decryptEmbedding(const std::string& b64Blob) {
    if (!s_keyLoaded)
        throw std::runtime_error("decryptEmbedding: encryption key not loaded");

    constexpr int IV_LEN  = 12;
    constexpr int TAG_LEN = 16;
    constexpr int MIN_LEN = IV_LEN + TAG_LEN;  // minimum valid blob

    std::vector<unsigned char> blob;
    try {
        blob = base64Decode(b64Blob);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("decryptEmbedding: base64 decode failed: ") + e.what());
    }

    if (static_cast<int>(blob.size()) < MIN_LEN)
        throw std::runtime_error("decryptEmbedding: blob too short to contain IV + tag");

    const unsigned char* iv         = blob.data();
    unsigned char        tag[TAG_LEN];
    std::memcpy(tag, blob.data() + IV_LEN, TAG_LEN);
    const unsigned char* cipherData = blob.data() + MIN_LEN;
    const int            cipherLen  = static_cast<int>(blob.size()) - MIN_LEN;

    std::vector<unsigned char> plaintext(cipherLen + EVP_MAX_BLOCK_LENGTH);
    int plainLen  = 0;
    int finalLen  = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("decryptEmbedding: EVP_CIPHER_CTX_new failed");

    auto cleanup = [&]() { EVP_CIPHER_CTX_free(ctx); };

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        cleanup(); throw std::runtime_error("decryptEmbedding: EVP_DecryptInit_ex (cipher) failed");
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, nullptr) != 1) {
        cleanup(); throw std::runtime_error("decryptEmbedding: set IV length failed");
    }
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, s_key.data(), iv) != 1) {
        cleanup(); throw std::runtime_error("decryptEmbedding: EVP_DecryptInit_ex (key/iv) failed");
    }
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &plainLen, cipherData, cipherLen) != 1) {
        cleanup(); throw std::runtime_error("decryptEmbedding: EVP_DecryptUpdate failed");
    }
    // Set the expected tag before calling Final
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag) != 1) {
        cleanup(); throw std::runtime_error("decryptEmbedding: failed to set GCM tag");
    }
    // Final returns -1 if tag verification fails
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + plainLen, &finalLen) != 1) {
        cleanup();
        throw std::runtime_error("decryptEmbedding: GCM authentication failed - "
                                 "data may be corrupt or tampered");
    }
    cleanup();

    const int totalPlain = plainLen + finalLen;
    return std::string(reinterpret_cast<const char*>(plaintext.data()), totalPlain);
}

// ─────────────────────────────────────────────────────────────────────────────
// enroll
// Change 3: generate embeddings at enrollment time.
// Change 4: accept multiple photos.
// Change 6: store only encrypted embeddings, not raw photos.
// ─────────────────────────────────────────────────────────────────────────────

json FaceController::enroll(const std::string& userId,
                            const std::string& electionId,
                            const std::string& voterId,
                            const std::vector<std::string>& photos) {
    json res;

    // Auth: verify election belongs to this user
    auto own = supabaseRequest("GET",
        "elections?select=id&id=eq."+electionId+"&user_id=eq."+userId+"&limit=1");
    try {
        auto arr = json::parse(own.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"] = false; res["message"] = "Unauthorized"; return res;
        }
    } catch (...) {
        res["success"] = false; res["message"] = "Server error"; return res;
    }

    // Verify voter exists in this election
    auto vcheck = supabaseRequest("GET",
        "voters?select=id&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");
    try {
        auto arr = json::parse(vcheck.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"] = false; res["message"] = "Voter not found"; return res;
        }
    } catch (...) {
        res["success"] = false; res["message"] = "Server error"; return res;
    }

    if (photos.empty() || photos.size() > 3) {
        res["success"] = false;
        res["message"] = "Provide 1–3 photos (front, left, right)";
        return res;
    }

    // Send photos to Python service for embedding generation
    json reqBody;
    reqBody["photos"] = photos;
    auto faceRes = callFaceService("/generate-embedding", reqBody);

    if (!faceRes.value("success", false)) {
        res["success"] = false;
        res["message"] = faceRes.value("message", "Failed to generate face embeddings");
        return res;
    }

    // Change 6: encrypt embeddings before storing
    std::string embeddingJson = faceRes["embeddings"].dump();
    std::string encrypted     = encryptEmbedding(embeddingJson);

    // Check if record already exists (re-enrollment case)
    auto existing = supabaseRequest("GET",
        "voter_embeddings?select=id&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");

    bool alreadyExists = false;
    try {
        auto arr = json::parse(existing.body);
        alreadyExists = arr.is_array() && !arr.empty();
    } catch (...) {}

    json dbBody;
    dbBody["election_id"]     = electionId;
    dbBody["voter_id"]        = voterId;
    dbBody["embeddings_json"] = encrypted;
    dbBody["embedding_count"] = faceRes.value("count", 1);

    HttpResult dbRes;
    if (alreadyExists) {
        // Re-enrollment: UPDATE existing record
        dbRes = supabaseRequest("PATCH",
            "voter_embeddings?election_id=eq."+electionId+
            "&voter_id=eq."+SupabaseClient::urlEncode(voterId),
            dbBody.dump());
    } else {
        // First enrollment: INSERT new record
        dbRes = supabaseRequest("POST", "voter_embeddings", dbBody.dump());
    }

    if (dbRes.statusCode != 200 && dbRes.statusCode != 201 && dbRes.statusCode != 204) {
        LOG_ERROR("[FaceController] Failed to store embedding. Status: " +
                  std::to_string(dbRes.statusCode) + " Body: " + dbRes.body);
        res["success"] = false;
        res["message"] = "Failed to store face data";
        return res;
    }

    // Change 6: photos are NOT stored - only embeddings reach the DB
    res["success"] = true;
    res["message"] = (alreadyExists ? "Face re-enrolled successfully (" : "Face enrolled successfully (") +
                     std::to_string(faceRes.value("count",1)) + " embedding(s) stored)";
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// verify
// Change 1: C++ fetches embeddings from DB, Python service stays stateless.
// Change 2: configurable threshold.
// Change 5: browser sends all captured frames; server-side liveness.analyse_frames()
//           validates the sequence and selects the best frame.
// ─────────────────────────────────────────────────────────────────────────────

json FaceController::verify(const std::string& electionId,
                            const std::string& voterId,
                            const std::vector<std::string>& frames,
                            float threshold) {
    json res;

    if (frames.empty()) {
        res["success"]  = false;
        res["verified"] = false;
        res["message"]  = "No frames received for verification";
        return res;
    }

    // Change 1: fetch stored embeddings from Supabase (not done in Python service)
    auto emRes = supabaseRequest("GET",
        "voter_embeddings?select=embeddings_json,embedding_count"
        "&election_id=eq."+electionId+
        "&voter_id=eq."+SupabaseClient::urlEncode(voterId)+"&limit=1");

    std::string encryptedEmb;
    try {
        auto arr = json::parse(emRes.body);
        if (!arr.is_array() || arr.empty()) {
            res["success"]  = false;
            res["verified"] = false;
            res["message"]  = "No face data enrolled for this voter";
            return res;
        }
        encryptedEmb = arr[0]["embeddings_json"].get<std::string>();
    } catch (...) {
        res["success"] = false; res["message"] = "Server error"; return res;
    }

    // Decrypt embeddings
    std::string embeddingJson = decryptEmbedding(encryptedEmb);
    json storedEmbeddings;
    try {
        storedEmbeddings = json::parse(embeddingJson);
    } catch (...) {
        res["success"] = false; res["message"] = "Corrupted face data"; return res;
    }

    // Build request for Python service.
    // Send the full frame sequence - liveness.analyse_frames() runs server-side.
    json reqBody;
    reqBody["frames"]            = frames;           // full sequence for liveness check
    reqBody["stored_embeddings"] = storedEmbeddings; // Change 1: passed from C++
    if (threshold > 0.0f) {
        reqBody["threshold"] = threshold;            // Change 2: optional override
    }

    auto faceRes = callFaceService("/verify", reqBody);

    bool verified = faceRes.value("verified", false);
    float score   = faceRes.value("score", 0.0f);

    LOG_INFO("[FaceController] verify voter=" + voterId +
             " score=" + std::to_string(score) +
             " verified=" + (verified ? "true" : "false"));

    res["success"]        = true;
    res["verified"]       = verified;
    res["score"]          = score;
    res["threshold_used"] = faceRes.value("threshold_used", threshold);
    if (!verified) {
        res["message"] = faceRes.value("reason", "Face verification failed");
    }
    return res;
}
