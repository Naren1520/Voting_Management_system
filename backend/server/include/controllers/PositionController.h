#pragma once
#include "../../third_party/json.hpp"
#include <string>

using json = nlohmann::json;

class PositionController {
public:
    bool ownsElection(const std::string& userId, const std::string& electionId);

    json getPositions(const std::string& userId, const std::string& electionId);

    json addPosition(const std::string& userId, const std::string& electionId,
                     const std::string& title);

    json deletePosition(const std::string& userId, const std::string& electionId,
                        const std::string& positionId);

    json getCandidates(const std::string& userId, const std::string& electionId,
                       const std::string& positionId);

    json addCandidate(const std::string& userId, const std::string& electionId,
                      const std::string& positionId, const std::string& name);

    json deleteCandidate(const std::string& userId, const std::string& electionId,
                         const std::string& positionId, const std::string& name);
};
