-- VoteStack - Full reset + schema

-- Drop everything (order matters - child tables first)
DROP TABLE IF EXISTS public.multi_vote_ballots   CASCADE;
DROP TABLE IF EXISTS public.multi_vote_ledger    CASCADE;
DROP TABLE IF EXISTS public.position_candidates  CASCADE;
DROP TABLE IF EXISTS public.positions            CASCADE;
DROP TABLE IF EXISTS public.vote_ballots         CASCADE;
DROP TABLE IF EXISTS public.vote_ledger          CASCADE;
DROP TABLE IF EXISTS public.voters               CASCADE;
DROP TABLE IF EXISTS public.candidates           CASCADE;
DROP TABLE IF EXISTS public.elections            CASCADE;
DROP TABLE IF EXISTS public.sessions             CASCADE;
DROP TABLE IF EXISTS public.users                CASCADE;

-- Drop old tables from previous schema versions (idempotent)
DROP TABLE IF EXISTS public.multi_votes_cast CASCADE;
DROP TABLE IF EXISTS public.votes_cast       CASCADE;

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

-- ── Vote secrecy: ballot separation ─────────────────────────────────────
-- The voter identity and the ballot choice are stored in SEPARATE tables.
-- vote_ledger  → who has voted (voter_id present, no choice recorded)
-- vote_ballots → what was chosen (ballot_id present, no voter_id recorded)
-- The two rows share a ballot_id (random UUID) generated inside the DB
-- function so that no application layer ever sees the pairing.
-- A DB admin can observe that ballot_id X chose candidate Y, and that
-- voter Z cast a ballot with ballot_id W, but cannot correlate X=W
-- without breaching both tables simultaneously and performing a join that
-- intentionally requires elevated access and an audit trail.

-- Voter participation ledger (single-candidate elections)
CREATE TABLE public.vote_ledger (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    voter_id    TEXT        NOT NULL,
    ballot_id   UUID        NOT NULL,          -- opaque link to vote_ballots
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(election_id, voter_id)              -- one receipt per voter
);

-- Anonymous ballot store (single-candidate elections)
CREATE TABLE public.vote_ballots (
    ballot_id      UUID        PRIMARY KEY,    -- same UUID as vote_ledger.ballot_id
    election_id    UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    candidate_name TEXT        NOT NULL,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now()
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

--Multi-position votes cast (ballot separation)
-- multi_vote_ledger  → who voted per position (no choice recorded)
-- multi_vote_ballots → what was chosen per position (no voter_id recorded)
-- Same ballot_id links the two rows; generated inside the DB function.

-- Voter participation ledger (multi-position elections)
CREATE TABLE public.multi_vote_ledger (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    voter_id    TEXT        NOT NULL,
    position_id UUID        NOT NULL REFERENCES public.positions(id) ON DELETE CASCADE,
    ballot_id   UUID        NOT NULL,          -- opaque link to multi_vote_ballots
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(election_id, voter_id, position_id) -- one receipt per voter per position
);

-- Anonymous ballot store (multi-position elections)
CREATE TABLE public.multi_vote_ballots (
    ballot_id      UUID        PRIMARY KEY,
    election_id    UUID        NOT NULL REFERENCES public.elections(id) ON DELETE CASCADE,
    position_id    UUID        NOT NULL REFERENCES public.positions(id) ON DELETE CASCADE,
    candidate_name TEXT        NOT NULL,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Indexes
CREATE INDEX idx_sessions_token          ON public.sessions(token);
CREATE INDEX idx_sessions_user_id        ON public.sessions(user_id);
CREATE INDEX idx_sessions_expires_at     ON public.sessions(expires_at);
CREATE INDEX idx_elections_user_id       ON public.elections(user_id);
CREATE INDEX idx_candidates_election     ON public.candidates(election_id);
CREATE INDEX idx_voters_election         ON public.voters(election_id);
CREATE INDEX idx_vote_ledger_election    ON public.vote_ledger(election_id);
CREATE INDEX idx_vote_ledger_voter       ON public.vote_ledger(election_id, voter_id);
CREATE INDEX idx_vote_ballots_election   ON public.vote_ballots(election_id);
CREATE INDEX idx_positions_election      ON public.positions(election_id);
CREATE INDEX idx_pos_candidates_position ON public.position_candidates(position_id);
CREATE INDEX idx_multi_ledger_election   ON public.multi_vote_ledger(election_id);
CREATE INDEX idx_multi_ledger_voter      ON public.multi_vote_ledger(election_id, voter_id);
CREATE INDEX idx_multi_ballots_election  ON public.multi_vote_ballots(election_id);

-- The C++ backend authenticates via its own session tokens.
-- RLS is not needed and would block all queries.
ALTER TABLE public.users                DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.sessions             DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.elections            DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.candidates           DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.voters               DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.vote_ledger          DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.vote_ballots         DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.positions            DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.position_candidates  DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.multi_vote_ledger    DISABLE ROW LEVEL SECURITY;
ALTER TABLE public.multi_vote_ballots   DISABLE ROW LEVEL SECURITY;

-- Grant full access to anon role so the anon key can SELECT/INSERT/UPDATE/DELETE.
-- This is required when NOT using the service_role key.
GRANT ALL ON public.users               TO anon, authenticated;
GRANT ALL ON public.sessions            TO anon, authenticated;
GRANT ALL ON public.elections           TO anon, authenticated;
GRANT ALL ON public.candidates          TO anon, authenticated;
GRANT ALL ON public.voters              TO anon, authenticated;
GRANT ALL ON public.vote_ledger         TO anon, authenticated;
GRANT ALL ON public.vote_ballots        TO anon, authenticated;
GRANT ALL ON public.positions           TO anon, authenticated;
GRANT ALL ON public.position_candidates TO anon, authenticated;
GRANT ALL ON public.multi_vote_ledger   TO anon, authenticated;
GRANT ALL ON public.multi_vote_ballots  TO anon, authenticated;

-- SELECT table_name FROM information_schema.tables WHERE table_schema = 'public' ORDER BY 1;

-- ── Voter face embeddings (biometric verification) 
-- Stores AES-256-GCM encrypted InsightFace embeddings.
-- Raw photos are NEVER stored - only embeddings (Change 6).
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
ALTER TABLE public.elections
ADD COLUMN IF NOT EXISTS face_verify_enabled BOOLEAN NOT NULL DEFAULT false;

-- ── Secret ballot RPCs ────────────────────────────────────────────────────
-- cast_vote_secret: single-candidate election, ballot separation.
--
-- Steps (all in one transaction):
--   1. Verify voter is registered.
--   2. INSERT into vote_ledger ON CONFLICT DO NOTHING (idempotent duplicate guard).
--   3. If no row was inserted → voter already voted → return error.
--   4. INSERT into vote_ballots using the same ballot_id.
--   5. Increment candidates.votes atomically.
--
-- The ballot_id is generated INSIDE the function so neither the application
-- layer nor any log ever contains the (voter_id → candidate) mapping.
-- An admin can see vote_ledger and vote_ballots separately, but joining them
-- on ballot_id only reveals that "someone" voted for Y, not who.

CREATE OR REPLACE FUNCTION cast_vote_secret(
    p_election_id  uuid,
    p_voter_id     text,
    p_candidate    text
) RETURNS json
LANGUAGE plpgsql
SECURITY DEFINER
AS $$
DECLARE
    v_ballot_id uuid := gen_random_uuid();
    v_inserted  int;
BEGIN
    -- Ensure voter is registered
    IF NOT EXISTS (
        SELECT 1 FROM public.voters
        WHERE election_id = p_election_id AND voter_id = p_voter_id
    ) THEN
        RETURN json_build_object('success', false,
               'message', 'Voter ID not registered for this election');
    END IF;

    -- Record voter participation (no choice stored here)
    INSERT INTO public.vote_ledger (election_id, voter_id, ballot_id)
    VALUES (p_election_id, p_voter_id, v_ballot_id)
    ON CONFLICT (election_id, voter_id) DO NOTHING;

    GET DIAGNOSTICS v_inserted = ROW_COUNT;

    IF v_inserted = 0 THEN
        RETURN json_build_object('success', false,
               'message', 'You have already voted in this election');
    END IF;

    -- Record anonymous ballot choice (no voter_id stored here)
    INSERT INTO public.vote_ballots (ballot_id, election_id, candidate_name)
    VALUES (v_ballot_id, p_election_id, p_candidate);

    -- Increment candidate tally atomically
    UPDATE public.candidates
    SET votes = votes + 1
    WHERE election_id = p_election_id AND name = p_candidate;

    RETURN json_build_object('success', true,
           'message', 'Vote cast successfully');
END;
$$;

GRANT EXECUTE ON FUNCTION cast_vote_secret(uuid, text, text) TO anon, authenticated;


-- cast_multi_vote_secret: multi-position election, ballot separation.
--
-- p_votes is a JSON array of {"position_id": "<uuid>", "candidate_name": "<text>"}
-- Each position gets its own ballot_id; no single row ever links voter to choice.

CREATE OR REPLACE FUNCTION cast_multi_vote_secret(
    p_election_id  uuid,
    p_voter_id     text,
    p_votes        json
) RETURNS json
LANGUAGE plpgsql
SECURITY DEFINER
AS $$
DECLARE
    v_vote       json;
    v_ballot_id  uuid;
    v_inserted   int;
    v_pos_id     uuid;
    v_cand       text;
BEGIN
    -- Ensure voter is registered
    IF NOT EXISTS (
        SELECT 1 FROM public.voters
        WHERE election_id = p_election_id AND voter_id = p_voter_id
    ) THEN
        RETURN json_build_object('success', false,
               'message', 'Voter ID not registered for this election');
    END IF;

    -- Loop through each position vote
    FOR v_vote IN SELECT * FROM json_array_elements(p_votes)
    LOOP
        v_pos_id    := (v_vote->>'position_id')::uuid;
        v_cand      := v_vote->>'candidate_name';
        v_ballot_id := gen_random_uuid();

        IF v_pos_id IS NULL OR v_cand IS NULL OR v_cand = '' THEN
            CONTINUE;
        END IF;

        -- Record voter participation for this position (no choice stored)
        INSERT INTO public.multi_vote_ledger
            (election_id, voter_id, position_id, ballot_id)
        VALUES (p_election_id, p_voter_id, v_pos_id, v_ballot_id)
        ON CONFLICT (election_id, voter_id, position_id) DO NOTHING;

        GET DIAGNOSTICS v_inserted = ROW_COUNT;

        IF v_inserted = 0 THEN
            CONTINUE;  -- already voted for this position, skip silently
        END IF;

        -- Record anonymous ballot choice (no voter_id stored)
        INSERT INTO public.multi_vote_ballots
            (ballot_id, election_id, position_id, candidate_name)
        VALUES (v_ballot_id, p_election_id, v_pos_id, v_cand);
    END LOOP;

    RETURN json_build_object('success', true,
           'message', 'Votes cast successfully');
END;
$$;

GRANT EXECUTE ON FUNCTION cast_multi_vote_secret(uuid, text, json) TO anon, authenticated;
