#include "../../include/controllers/FaceController.h"
#include "../../include/db/SupabaseClient.h"
#include "../../include/core/Logger.h"
#include "../../include/core/Config.h"

#include <curl/curl.h>
#include <cstdlib>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// libcurl write callback
// ─────────────────────────────────────────────────────────────────────────────

static size_t faceWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = reinterpret_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// ─────────────────────────────────────────────────────────────────────────────
// callFaceService — direct libcurl call to Modal (or any external URL)
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        90L);   // 90s — Modal cold start
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
// encryptEmbedding / decryptEmbedding
// Change 6: embeddings stored encrypted in DB.
// Simple XOR-based placeholder — replace with AES-256-GCM in production.
// ─────────────────────────────────────────────────────────────────────────────

std::string FaceController::encryptEmbedding(const std::string& plain) {
    // TODO: replace with AES-256-GCM using OpenSSL EVP_EncryptInit_ex
    // For now, base64-encode as a placeholder (not real encryption)
    // Real implementation:
    //   EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    //   EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv);
    //   ...
    LOG_WARN("[FaceController] encryptEmbedding: using placeholder (implement AES-256-GCM)");
    return plain; // placeholder — implement before production
}

std::string FaceController::decryptEmbedding(const std::string& encrypted) {
    LOG_WARN("[FaceController] decryptEmbedding: using placeholder (implement AES-256-GCM)");
    return encrypted; // placeholder
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

    // Store in voter_embeddings table
    json dbBody;
    dbBody["election_id"]        = electionId;
    dbBody["voter_id"]           = voterId;
    dbBody["embeddings_json"]    = encrypted;
    dbBody["embedding_count"]    = faceRes.value("count", 1);

    auto dbRes = supabaseRequest("POST", "voter_embeddings", dbBody.dump());
    if (dbRes.statusCode != 200 && dbRes.statusCode != 201) {
        LOG_ERROR("[FaceController] Failed to store embedding. Status: " +
                  std::to_string(dbRes.statusCode));
        res["success"] = false;
        res["message"] = "Failed to store face data";
        return res;
    }

    // Change 6: photos are NOT stored — only embeddings reach the DB
    res["success"] = true;
    res["message"] = "Face enrolled successfully (" +
                     std::to_string(faceRes.value("count",1)) + " embedding(s) stored)";
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// verify
// Change 1: C++ fetches embeddings from DB, Python service stays stateless.
// Change 2: configurable threshold.
// Change 5: best_frame already selected by browser liveness logic.
// ─────────────────────────────────────────────────────────────────────────────

json FaceController::verify(const std::string& electionId,
                            const std::string& voterId,
                            const std::string& bestFrameBase64,
                            float threshold) {
    json res;

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

    // Build request for Python service
    json reqBody;
    reqBody["best_frame"]         = bestFrameBase64;
    reqBody["stored_embeddings"]  = storedEmbeddings;  // Change 1: passed from C++
    if (threshold > 0.0f) {
        reqBody["threshold"] = threshold;  // Change 2: optional override
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
