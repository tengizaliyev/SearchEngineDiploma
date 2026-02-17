DROP TABLE IF EXISTS word_frequency;
DROP TABLE IF EXISTS words;
DROP TABLE IF EXISTS documents;

CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    url TEXT UNIQUE,
    content TEXT
);

CREATE TABLE words (
    id SERIAL PRIMARY KEY,
    word TEXT UNIQUE
);

CREATE TABLE word_frequency (
    document_id INT REFERENCES documents(id),
    word_id INT REFERENCES words(id),
    count INT,
    PRIMARY KEY(document_id, word_id)
);
