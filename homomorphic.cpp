#include "seal/seal.h"
#include <iostream>
#include "seal/native/src/seal/util/iterator.h"
#include "seal/native/src/seal/util/rlwe.h"

using namespace std;
using namespace seal;

class Homomorphic {
    private:
        unique_ptr<SEALContext> context;
    public:
        // Initialization, set the modulus value
        Homomorphic(size_t security) {
            EncryptionParameters parms(scheme_type::bfv);
            parms.set_poly_modulus_degree(security);
            parms.set_coeff_modulus(CoeffModulus::BFVDefault(security));
            parms.set_plain_modulus(1024);

            context = make_unique<SEALContext>(parms);
        }

        // Using seal/native/src/seal/keygenerator.cpp as reference
        // from line 67 to 81
        void generateKeys(int id, SecretKey& sk, PublicKey& pk) {
            array<uint64_t, 8> seed = { id, 0, 0, 0, 0, 0, 0, 0 };
            auto prng = Blake2xbPRNGFactory(seed).create();

            sk = SecretKey();
            auto& parms = (*context->key_context_data()).parms();
            sk.data().resize(parms.poly_modulus_degree() * parms.coeff_modulus().size());

            util::RNSIter secret_key(sk.data().data(), parms.poly_modulus_degree());
            util::sample_poly_ternary(prng, parms, secret_key);

            sk.parms_id() = (*context->key_context_data()).parms_id();

            KeyGenerator keygen(*context, sk);
            keygen.create_public_key(pk);
            return; 
        }

        void encryptSecret(PublicKey pk, int secret) {
            Evaluator evaluator(*context);
            Encryptor encryptor(*context, pk);

            Plaintext plaintext(to_string(secret));

            Ciphertext enc;
            encryptor.encrypt(plaintext, enc);
        }

        void decryptSecret(SecretKey sk, Plaintext& result) {
            Evaluator evaluator(*context);
            Decryptor decryptor(*context, sk);

            Ciphertext res;
            Plaintext plain_result;
            decryptor.decrypt(res, plain_result);

            result = plain_result.to_string();

            return;
        }

        void addSecrets(Ciphertext a, Ciphertext b, Ciphertext& res) {
            Evaluator evaluator(*context);
            evaluator.add(a, b, res);
            return;
        }
};