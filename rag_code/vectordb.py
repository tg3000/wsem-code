import numpy as np

class VectorDB:
    def __init__(self):
        self.texts = np.empty(0, dtype=object)
        self.vectors = np.empty((0, 0), dtype=float)

    @staticmethod
    def normalize(vector):
        vector = np.asarray(vector, dtype=float)
        if vector.ndim != 1:
            raise ValueError("vector must be a 1-dimensional numpy array")

        length_squared = np.dot(vector, vector)
        if length_squared == 0:
            return vector
        return vector / np.sqrt(length_squared)

    def store(self, text, vector):
        vector = np.asarray(vector, dtype=float)
        if vector.ndim != 1:
            raise ValueError("vector must be a 1-dimensional numpy array")

        if self.vectors.shape[0] == 0:
            self.vectors = VectorDB.normalize(vector).reshape(1, -1)
        else:
            if vector.shape[0] != self.vectors.shape[1]:
                raise ValueError(
                    f"vector has length {vector.shape[0]}, expected {self.vectors.shape[1]}"
                )
            self.vectors = np.vstack([self.vectors, VectorDB.normalize(vector)])

        self.texts = np.append(self.texts, text)

    def search(self, vector: np.array, top=1):
        if vector.ndim != 1:
            raise ValueError("vector must be a 1-dimensional numpy array")
        if len(self.texts) == 0:
            raise ValueError("no values in databank yet")
        vector = VectorDB.normalize(vector)
        cos_similarities = np.dot(self.vectors, vector)

        k = min(top, len(cos_similarities))
        top_indices = np.argsort(cos_similarities)[::-1][:k]
        for i in top_indices:
            yield {"text": self.texts[i], "score": cos_similarities[i]}

    def scores_sorted(self, vector: np.array):
        if vector.ndim != 1:
            raise ValueError("vector must be a 1-dimensional numpy array")
        if len(self.texts) == 0:
            raise ValueError("no values in databank yet")
        vector = VectorDB.normalize(vector)
        cos_similarities = np.dot(self.vectors, vector)

        top_indices = np.argsort(cos_similarities)[::-1]
        scores = []
        for i in top_indices:
            scores.append({"text": self.texts[i], "score": cos_similarities[i]})
        return scores

    def scores_unsorted(self, vector: np.array):
        if vector.ndim != 1:
            raise ValueError("vector must be a 1-dimensional numpy array")
        if len(self.texts) == 0:
            raise ValueError("no values in databank yet")
        vector = VectorDB.normalize(vector)
        cos_similarities = np.dot(self.vectors, vector)

        scores = []
        for i in range(len(cos_similarities)):
            scores.append({"text": self.texts[i], "score": cos_similarities[i]})
        return scores


        


