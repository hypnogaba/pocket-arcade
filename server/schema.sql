CREATE TABLE IF NOT EXISTS scores (
  user  TEXT PRIMARY KEY,   -- X-Badge-User: the badge owner's id
  badge TEXT NOT NULL,      -- X-Badge-Id (emulators start with "e")
  name  TEXT NOT NULL,      -- what the player chose to show
  score INTEGER NOT NULL,
  stage INTEGER NOT NULL,
  epoch INTEGER NOT NULL,
  at    INTEGER NOT NULL    -- unix ms of the best score
);
CREATE INDEX IF NOT EXISTS scores_by_score ON scores (score DESC, at ASC);
