BearSSL 0.6（https://bearssl.org/、MIT ライセンス、LICENSE.txt）のうち、
更新アプリの HTTPS（https.c。br_ssl_client_init_full と br_x509_minimal）に要るファイルだけを、
手を加えずに写したもの。

元: Ubuntu の bearssl_0.6+dfsg.1.orig.tar.xz（上流の 0.6 から T0Comp.exe だけを除いたもの）
    SHA-256 102a625bd8f6155260867a50c508df8d8c3f796985dd744df1a68e5cd1a79004
  inc/*.h、src/inner.h、src/config.h と、src/ の下の次のファイルを、フォルダを分けずに置いた。
  aead/ccm.c
  codec/ccopy.c dec32be.c dec32le.c dec64be.c enc32be.c enc32le.c enc64be.c
  ec/ec_all_m31.c ec_c25519_m31.c ec_p256_m31.c ec_prime_i31.c ec_secp256r1.c ec_secp384r1.c
     ec_secp521r1.c ecdsa_atr.c ecdsa_i31_bits.c ecdsa_i31_vrfy_asn1.c ecdsa_i31_vrfy_raw.c
  hash/ghash_ctmul.c md5.c multihash.c sha1.c sha2big.c sha2small.c
  int/i31_add.c i31_bitlen.c i31_decmod.c i31_decode.c i31_encode.c i31_fmont.c i31_iszero.c
      i31_modpow.c i31_modpow2.c i31_montmul.c i31_muladd.c i31_ninv31.c i31_rshift.c i31_sub.c
      i31_tmont.c i32_div32.c
  mac/hmac.c hmac_ct.c
  rand/hmac_drbg.c sysrng.c
  rsa/rsa_default_pkcs1_vrfy.c rsa_default_pub.c rsa_i31_pkcs1_vrfy.c rsa_i31_pub.c
      rsa_pkcs1_sig_unpad.c
  ssl/prf.c prf_md5sha1.c prf_sha256.c prf_sha384.c ssl_client.c ssl_client_default_rsapub.c
      ssl_client_full.c ssl_engine.c ssl_engine_default_aescbc.c ssl_engine_default_aesccm.c
      ssl_engine_default_aesgcm.c ssl_engine_default_chapol.c ssl_engine_default_descbc.c
      ssl_engine_default_ecdsa.c ssl_engine_default_rsavrfy.c ssl_hs_client.c ssl_io.c
      ssl_rec_cbc.c ssl_rec_ccm.c ssl_rec_chapol.c ssl_rec_gcm.c
  symcipher/aes_ct.c aes_ct_cbcdec.c aes_ct_cbcenc.c aes_ct_ctr.c aes_ct_ctrcbc.c aes_ct_dec.c
      aes_ct_enc.c chacha20_ct.c des_ct.c des_ct_cbcdec.c des_ct_cbcenc.c des_support.c
      poly1305_ctmul.c
  x509/x509_minimal.c

Rosalina（sysmodules/rosalina/source/bearssl/）にも、起動時の確認に要る分だけを置いている。
