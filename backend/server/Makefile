CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -pthread
LDFLAGS  := -lcurl -lssl -lcrypto -lpthread

SRCDIR   := src
INCDIR   := include
BINDIR   := bin
TARGET   := $(BINDIR)/voting_server

SRCS := $(SRCDIR)/main.cpp \
        $(SRCDIR)/core/Config.cpp \
        $(SRCDIR)/core/Logger.cpp \
        $(SRCDIR)/core/ThreadPool.cpp \
        $(SRCDIR)/net/EpollServer.cpp \
        $(SRCDIR)/net/HttpRequest.cpp \
        $(SRCDIR)/net/HttpResponse.cpp \
        $(SRCDIR)/db/SupabaseClient.cpp \
        $(SRCDIR)/cache/RedisClient.cpp \
        $(SRCDIR)/cache/CandidateCache.cpp \
        $(SRCDIR)/controllers/AuthController.cpp \
        $(SRCDIR)/controllers/ElectionController.cpp \
        $(SRCDIR)/controllers/CandidateController.cpp \
        $(SRCDIR)/controllers/VoterController.cpp \
        $(SRCDIR)/controllers/PositionController.cpp \
        $(SRCDIR)/controllers/PublicVoteController.cpp \
        $(SRCDIR)/controllers/PublicMultiVoteController.cpp

OBJS := $(patsubst $(SRCDIR)/%.cpp, $(BINDIR)/obj/%.o, $(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build successful: $@"

$(BINDIR)/obj/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -Ithird_party -c $< -o $@

clean:
	rm -rf $(BINDIR)

run: all
	SUPABASE_URL=$${SUPABASE_URL} SUPABASE_KEY=$${SUPABASE_KEY} ./$(TARGET)
