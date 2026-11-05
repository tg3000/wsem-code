#include "openfhe.h"
#include "json.hpp"

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace lbcrypto;
using namespace std;
using json = nlohmann::json;

#ifndef FHE_DATA_DIR
#define FHE_DATA_DIR "."
#endif

struct Record {
    string text;
    vector<double> embed;
};

static string data_path(const string& name) {
    return string(FHE_DATA_DIR) + "/" + name;
}

static uint32_t next_pow2(uint32_t n) {
    uint32_t p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

static vector<double> pad_slots(const vector<double>& v, uint32_t slots) {
    vector<double> out(slots, 0.0);
    copy(v.begin(), v.end(), out.begin());
    return out;
}

static string record_text(const json& obj, initializer_list<const char*> keys) {
    for (const char* key : keys) {
        if (obj.contains(key) && obj[key].is_string())
            return obj[key].get<string>();
    }
    return {};
}

static vector<Record> load_records(const string& path, initializer_list<const char*> text_keys) {
    ifstream in(path);
    if (!in)
        throw runtime_error("cannot open " + path);
    json j;
    in >> j;
    vector<Record> recs;
    recs.reserve(j.size());
    for (const auto& obj : j) {
        Record r;
        r.text = record_text(obj, text_keys);
        if (obj.contains("embed"))
            r.embed = obj["embed"].get<vector<double>>();
        recs.push_back(std::move(r));
    }
    return recs;
}

class VectorDB {
public:
    explicit VectorDB(CryptoContext<DCRTPoly> context) : cc(std::move(context)) {}

    void load(const string& path) {
        auto recs = load_records(path, {"data", "answer", "text"});
        texts.clear();
        embeds.clear();
        enc_embeds.clear();
        for (auto& r : recs) {
            if (r.embed.empty())
                throw runtime_error("server record missing embed");
            texts.push_back(std::move(r.text));
            embeds.push_back(std::move(r.embed));
        }
        if (embeds.empty())
            throw runtime_error("empty server_data");
    }

    uint32_t dim() const {
        return static_cast<uint32_t>(embeds.front().size());
    }

    size_t size() const {
        return texts.size();
    }

    const string& text(size_t i) const {
        return texts.at(i);
    }

    void encrypt_store(const PublicKey<DCRTPoly>& pk, uint32_t slots) {
        enc_embeds.clear();
        enc_embeds.reserve(embeds.size());
        for (const auto& v : embeds) {
            Plaintext ptx = cc->MakeCKKSPackedPlaintext(pad_slots(v, slots));
            enc_embeds.push_back(cc->Encrypt(pk, ptx));
        }
        batch_slots = slots;
    }

    vector<Ciphertext<DCRTPoly>> scores(const Ciphertext<DCRTPoly>& enc_query) const {
        if (enc_embeds.empty())
            throw runtime_error("call encrypt_store before scores");
        vector<Ciphertext<DCRTPoly>> out;
        out.reserve(enc_embeds.size());
        for (const auto& row : enc_embeds)
            out.push_back(cc->EvalInnerProduct(enc_query, row, batch_slots));
        return out;
    }

private:
    CryptoContext<DCRTPoly> cc;
    vector<string> texts;
    vector<vector<double>> embeds;
    vector<Ciphertext<DCRTPoly>> enc_embeds;
    uint32_t batch_slots = 0;
};

static Ciphertext<DCRTPoly> encrypt_embed(const CryptoContext<DCRTPoly>& cc, const PublicKey<DCRTPoly>& pk, const vector<double>& embed, uint32_t slots) {
    Plaintext ptx = cc->MakeCKKSPackedPlaintext(pad_slots(embed, slots));
    return cc->Encrypt(pk, ptx);
}

static double decrypt_score(const CryptoContext<DCRTPoly>& cc, const PrivateKey<DCRTPoly>& sk, const Ciphertext<DCRTPoly>& ct) {
    Plaintext ptx;
    cc->Decrypt(sk, ct, &ptx);
    ptx->SetLength(1);
    return ptx->GetRealPackedValue()[0];
}

int main() {
    auto questions = load_records(data_path("client_questions.json"), {"question"});
    auto labels = load_records(data_path("labels.json"), {"answer"});
    auto server_recs = load_records(data_path("server_data.json"), {"data", "answer", "text"});
    if (server_recs.empty() || server_recs.front().embed.empty())
        throw runtime_error("empty server_data");

    uint32_t dim = static_cast<uint32_t>(server_recs.front().embed.size());
    uint32_t slots = next_pow2(dim);

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(1);
    parameters.SetScalingModSize(50);
    parameters.SetBatchSize(slots);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);

    VectorDB db(cc);
    db.load(data_path("server_data.json"));

    KeyPair<DCRTPoly> keyPair = cc->KeyGen();
    cc->EvalMultKeyGen(keyPair.secretKey);
    cc->EvalSumKeyGen(keyPair.secretKey);

    db.encrypt_store(keyPair.publicKey, slots);

    if (questions.size() != labels.size())
        throw runtime_error("client_questions.json and labels.json have different lengths");

    int correct = 0;
    cout.setf(ios::fixed);
    cout.precision(6);

    for (size_t q = 0; q < questions.size(); ++q) {
        auto enc_query = encrypt_embed(cc, keyPair.publicKey, questions[q].embed, slots);
        auto enc_scores = db.scores(enc_query);

        vector<double> scores(enc_scores.size());
        size_t best = 0;
        for (size_t i = 0; i < enc_scores.size(); ++i) {
            scores[i] = decrypt_score(cc, keyPair.secretKey, enc_scores[i]);
            if (scores[i] > scores[best])
                best = i;
        }

        const string& retrieved = db.text(best);
        const string& gold = labels[q].text;
        bool ok = retrieved == gold;
        if (ok)
            ++correct;

        cout << "Q: " << questions[q].text << "\n";
        for (size_t i = 0; i < scores.size(); ++i) {
            cout << "  [" << i << "] " << scores[i];
            if (i == best)
                cout << "  <- best";
            cout << "  " << db.text(i) << "\n";
        }
        cout << "  retrieved: " << retrieved << "\n"
             << "  label: " << gold << "\n"
             << "  " << (ok ? "OK" : "FAIL") << "\n";
    }

    cout << "correct " << correct << "/" << questions.size() << "\n";
    return correct == static_cast<int>(questions.size()) ? 0 : 1;
}
