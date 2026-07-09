-- VoteStack — Full reset + schema

-- Drop everything (order matters — child tables first)
DROP TABLE IF EXISTS public.multi_votes_cast    CASCADE;
DROP TABLE IF EXISTS public.position_candidates CASCADE;
DROP TABLE IF EXISTS public.positions           CASCADE;
DROP TABLE IF EXISTS public.votes_cast          CASCADE;
DROP TABLE IF EXISTS public.voters              CASCADE;
DROP TABLE IF EXISTS public.candidates          CASCADE;
DROP TABLE IF EXISTS public.elections           CASCADE;
DROP TABLE IF EXISTS public.sessions            CASCADE;
DROP TABLE IF EXISTS public.users               CASCADE;

-- Users
CREATE TABLE public.users (
    id            UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    name          TEXT        NOT NULL,
    email         TEXT        NOT NULL UNIQUE,
    password_hash TEXT        NOT NULL,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Sessions
CREATE TABLE public.sessions (
    id         UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    token      TEXT        NOT NULL UNIQUE,
    user_id    UUID        NOT NULL REFERENCES public.users(id) ON DELETE CASCADE,
    user_agent TEXT,
    ip_address TEXT,
    location   TEXT,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

--Elections 
CREATE TABLE public.elections (
    id            UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id       UUID        NOT NULL REFERENCES public.users(id) ON DELETE CASCADE,
    title         TEXT        NOT NULL,
    election_type TEXT        NOT NULL DEFAULT 'standard',
    is_active     BOOLEAN     NOT NULL DEFAULT true,
    schedule_type TEXT        NOT NULL DEFAULT 'always_on',
    starts_at     TIMESTAMPTZ,
    ends_at       TIMESTAMPTZ,
    schedule_json TEXT,
    timezone      TEXT        NOT NULL DEFAULT 'UTC',
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Candidates
CREATE TABLE public.candidates (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    name        TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(election_id, name)
);

-- Voters (registered voters per election)
CREATE TABLE public.voters (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    voter_id    TEXT        NOT NULL,
    name        TEXT,
    email       TEXT,
    phone       TEXT,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(election_id, voter_id)
);

-- Votes cast (single-candidate elections)
-- UNIQUE(election_id, voter_id) enforces one vote per voter at DB level
CREATE TABLE public.votes_cast (
    id             UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id    UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    voter_id       TEXT        NOT NULL,
    candidate_name TEXT        NOT NULL,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(election_id, voter_id)
);

-- Positions (multi-ballot elections) 
CREATE TABLE public.positions (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    title       TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

--  Position candidates 
CREATE TABLE public.position_candidates (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    position_id UUID        NOT NULL REFERENCES public.positions(id) ON DELETE CASCADE,
    name        TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(position_id, name)
);

--Multi-position votes cast
-- UNIQUE(election_id, voter_id, position_id) = one vote per position per voter
CREATE TABLE public.multi_votes_cast (
    id             UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id    UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    voter_id       TEXT        NOT NULL,
    position_id    UUID        NOT NULL REFERENCES public.positions(id) ON DELETE CASCADE,
    candidate_name TEXT        NOT NULL,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(election_id, voter_id, position_id)
);

-- Indexes
CREATE INDEX idx_sessions_token          ON public.sessions(token);
CREATE INDEX idx_sessions_user_id        ON public.sessions(user_id);
CREATE INDEX idx_sessions_expires_at     ON public.sessions(expires_at);
CREATE INDEX idx_elections_user_id       ON public.elections(user_id);
CREATE INDEX idx_candidates_election     ON public.candidates(election_id);
CREATE INDEX idx_voters_election         ON public.voters(election_id);
CREATE INDEX idx_votes_election          ON public.votes_cast(election_id);
CREATE INDEX idx_positions_election      ON public.positions(election_id);
CREATE INDEX idx_pos_candidates_position ON public.position_candidates(position_id);
CREATE INDEX idx_multi_votes_election    ON public.multi_votes_cast(election_id);

-- The C++ backend authenticates via its own session tokens.
-- RLS is not needed and would block all queries.
ALTER TABLE public.users                DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.sessions             DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.elections            DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.candidates           DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.voters               DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.votes_cast           DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.positions            DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.position_candidates  DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.multi_votes_cast     DISABLE ROW LEVEL SECURITY;

-- Grant full access to anon role so the anon key can SELECT/INSERT/UPDATE/DELETE.
-- This is required when NOT using the service_role key.
GRANT ALL ON public.users               TO anon, authenticated;
GRANT ALL ON public.sessions            TO anon, authenticated;
GRANT ALL ON public.elections           TO anon, authenticated;
GRANT ALL ON public.candidates          TO anon, authenticated;
GRANT ALL ON public.voters              TO anon, authenticated;
GRANT ALL ON public.votes_cast          TO anon, authenticated;
GRANT ALL ON public.positions           TO anon, authenticated;
GRANT ALL ON public.position_candidates TO anon, authenticated;
GRANT ALL ON public.multi_votes_cast    TO anon, authenticated;

-- SELECT table_name FROM information_schema.tables WHERE table_schema = 'public' ORDER BY 1;

-- ── Voter face embeddings (biometric verification) 
-- Stores AES-256-GCM encrypted InsightFace embeddings.
-- Raw photos are NEVER stored — only embeddings (Change 6).
CREATE TABLE IF NOT EXISTS public.voter_embeddings (
    id              UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id     UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    voter_id        TEXT        NOT NULL,
    embeddings_json TEXT        NOT NULL,  -- encrypted JSON array of float arrays
    embedding_count INT         NOT NULL DEFAULT 1,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(election_id, voter_id)
);

CREATE INDEX IF NOT EXISTS idx_face_embeddings_voter
    ON public.voter_embeddings(election_id, voter_id);

ALTER TABLE public.voter_embeddings DISABLE ROW LEVEL SECURITY;
GRANT ALL ON public.voter_embeddings TO anon, authenticated;

-- ── Face verification toggle per election ────────────────────────────────
-- Add this column to enable the face verification toggle in the manage page.
-- Default false = face verification off for existing elections.
ALTER TABLE public.elections
ADD COLUMN IF NOT EXISTS face_verify_enabled BOOLEAN NOT NULL DEFAULT false;
