CREATE TABLE IF NOT EXISTS documents (
    id SERIAL PRIMARY KEY,
    url TEXT UNIQUE,
    content TEXT
);

CREATE TABLE IF NOT EXISTS words (
    id SERIAL PRIMARY KEY,
    word TEXT UNIQUE
);

CREATE TABLE IF NOT EXISTS word_frequency (
    document_id INT REFERENCES documents(id) ON DELETE CASCADE,
    word_id INT REFERENCES words(id) ON DELETE CASCADE,
    count INT NOT NULL,
    PRIMARY KEY(document_id, word_id)
);
