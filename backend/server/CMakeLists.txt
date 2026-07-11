cmake_minimum_required(VERSION 3.16)
project(VotingServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# ── Find dependencies ─────────────────────────────────────────────────────────
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

#  Sources 
set(SOURCES
    src/main.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/ThreadPool.cpp
    src/net/EpollServer.cpp
    src/net/HttpRequest.cpp
    src/net/HttpResponse.cpp
    src/db/SupabaseClient.cpp
    src/cache/RedisClient.cpp
    src/cache/CandidateCache.cpp
    src/controllers/AuthController.cpp
    src/controllers/ElectionController.cpp
    src/controllers/CandidateController.cpp
    src/controllers/VoterController.cpp
    src/controllers/PositionController.cpp
    src/controllers/PublicVoteController.cpp
    src/controllers/PublicMultiVoteController.cpp
    src/controllers/FaceController.cpp
)

#  Target 
add_executable(voting_server ${SOURCES})

target_include_directories(voting_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(voting_server PRIVATE
    Threads::Threads
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

find_package(Threads REQUIRED)

set_target_properties(voting_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
