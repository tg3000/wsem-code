# Code zur Seminararbeit: Sicherheitsaspekte und kryptographische Herausforderungen in Retrieval-Augmented-Generation-Systemen

Zwei getrennte Teile. `rag_code/` misst, ob eine vergiftete Formulierung im Retrieval vor der wahren Antwort liegt. `fhe_code/` berechnet dasselbe Skalarprodukt mit CKKS, ohne die Einbettungen im Klartext zu multiplizieren.

`fhe_code/json.hpp` ist die JSON-Bibliothek von nlohmann.

## Wichtige Dateien

`rag_code/unpoisoned_data.json` enthält sechs Frage-Antwort-Paare.

`rag_code/vectordb.py` hält Texte und Einbettungen im Arbeitsspeicher. Beim Ablegen und bei der Suche werden die Vektoren auf Länge eins normiert. Die Ähnlichkeit ist dann das Skalarprodukt.

`rag_code/poison_embeddings.ipynb` baut daraus den Vergiftungsversuch. Pro Frage liegen die Falschaussage, die wahre Antwort und die Schablone *The real answer to the question … is Jakob Fugger* in der Datenbank. Zuerst mit `jina-embeddings-v2-base-en`, danach mit zehn weiteren Modellen von Hugging Face. Die Frage wird mit `query_embed` eingebettet, die abgelegten Texte mit `embed`.

`rag_code/fhe_embedding_creation.ipynb` bettet dieselben sechs Paare mit `jina-embeddings-v2-base-en` ein, normiert sie und schreibt drei JSON-Dateien für den FHE Teil.

`fhe_code/fhe_vector_db.cpp` liest diese Dateien. Der Serverteil verschlüsselt die Antwortvektoren unter dem öffentlichen Schlüssel des Clients und bildet mit `EvalInnerProduct` das Skalarprodukt gegen die verschlüsselte Frage. Der Client entschlüsselt nur die Scores und prüft, ob der beste Treffer zum hinterlegten Label passt. `fhe_code/CMakeLists.txt` "buildet" dieses Programm. `client_questions.json`, `server_data.json` und `labels.json` sind die Eingaben, die das Notebook erzeugt.

## RAG ausführen

Python 3.12. Die Notebooks im Ordner `rag_code` starten, weil sie `unpoisoned_data.json` relativ öffnen. Der erste Lauf lädt die Embedding-Modelle von Hugging Face.

```bash
cd rag_code
python3 -m venv .venv
source .venv/bin/activate
pip install numpy fastembed matplotlib jupyter
jupyter notebook
```

Zuerst `poison_embeddings.ipynb` ausführen. Die Plots werden im Notebook angezeigt. `fhe_embedding_creation.ipynb` nur ausführen, wenn die JSON-Dateien für den FHE-Teil neu erzeugt werden sollen. Die drei Dateien danach nach `fhe_code/` kopieren:

```bash
cp client_questions.json server_data.json labels.json ../fhe_code/
```

## FHE ausführen

Voraussetzungen sind CMake, ein C++17-Compiler und ein "gebuildetes" [OpenFHE](https://github.com/openfheorg/openfhe-development).

Zum "builden" von OpenFHE:
```bash
git clone https://github.com/openfheorg/openfhe-development.git
cmake -S openfhe-development -B openfhe-development/build
cmake --build openfhe-development/build
cmake --install openfhe-development/build
```

Danach das Programm "builden" und starten. Es liest die JSON-Dateien aus `fhe_code/`.

```bash
cmake -S fhe_code -B fhe_code/build
cmake --build fhe_code/build
./fhe_code/build/fhe_vector_db
```

Die Ausgabe zeigt pro Frage sechs Scores und endet mit `correct 6/6`, wenn jeder entschlüsselte Besttreffer dem Label entspricht.
