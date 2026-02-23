#include "seal/seal.h"
#include <iostream>
#include "seal/native/src/seal/util/iterator.h"
#include "seal/native/src/seal/util/rlwe.h"

using namespace std;
using namespace seal;

class Homomorphic {
    private:
    public:
        unique_ptr<SEALContext> context;
        Homomorphic() {
        }

        void setParameters(size_t security, int modulo) {
            EncryptionParameters parms(scheme_type::bfv);
            parms.set_poly_modulus_degree(security);
            parms.set_coeff_modulus(CoeffModulus::BFVDefault(security));
            parms.set_plain_modulus(modulo);

            context = make_unique<SEALContext>(parms);
        }

        // Using seal/native/src/seal/keygenerator.cpp as reference
        // from line 67 to 81
        void generateKeys(long unsigned int id, SecretKey& sk, PublicKey& pk) {
            array<uint64_t, 8> seed = { id, 0, 0, 0, 0, 0, 0, 0 };
            auto prng = Blake2xbPRNGFactory(seed).create();

            sk = SecretKey();
            auto& parms = (*context->key_context_data()).parms();
            sk.data().resize(parms.poly_modulus_degree() * parms.coeff_modulus().size());

            util::RNSIter secret_key(sk.data().data(), parms.poly_modulus_degree());
            util::sample_poly_ternary(prng, parms, secret_key);

            auto ntt_tables = (*context->key_context_data()).small_ntt_tables();
            util::ntt_negacyclic_harvey(secret_key, parms.coeff_modulus().size(), ntt_tables);

            sk.parms_id() = (*context->key_context_data()).parms_id();

            KeyGenerator keygen(*context, sk);
            keygen.create_public_key(pk);
            return; 
        }

        Ciphertext encryptSecret(PublicKey pk, int secret) {
            Encryptor encryptor(*context, pk);

            stringstream ss;
            ss << hex << secret;
            string hexeds = ss.str();

            Plaintext plaintext(hexeds);

            Ciphertext enc;
            encryptor.encrypt(plaintext, enc);

            return enc;
        }

        void decryptSecret(SecretKey sk, Ciphertext& enc, int& result) {
            Decryptor decryptor(*context, sk);

            Plaintext plain_result;
            decryptor.decrypt(enc, plain_result);

            result = stoi(plain_result.to_string(), nullptr, 16);
            return;
        }

        void addShares(Ciphertext a, Ciphertext b, Ciphertext& res) {
            Evaluator evaluator(*context);
            evaluator.add(a, b, res);
            return;
        }

        Ciphertext sumShares(vector<Ciphertext> shares) {
            Evaluator evaluator(*context);
            Ciphertext res;
            evaluator.add_many(shares, res);
            return res;
        }

        int getNoiseBudget(Ciphertext& encrypted, SecretKey secret_key) {
            Decryptor decryptor(*context, secret_key);

            int budget = decryptor.invariant_noise_budget(encrypted);
            return budget;
        }

        void loadCiphertext(Ciphertext& dest, istream& stream) {
            dest.load(*context, stream);
        }
};