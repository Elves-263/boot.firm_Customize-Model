BearSSL 0.6（https://bearssl.org/、MIT ライセンス、LICENSE.txt）のうち、
起動時の新しい版の確認（update_tls.c）に要るファイルだけを、手を加えずに写したもの。

元: Ubuntu の bearssl_0.6+dfsg.1.orig.tar.xz（上流の 0.6 から T0Comp.exe だけを除いたもの）
    SHA-256 102a625bd8f6155260867a50c508df8d8c3f796985dd744df1a68e5cd1a79004
  inc/*.h、src/inner.h、src/config.h と、src/ の下の次のファイルを、フォルダを分けずに置いた。
  codec/ccopy.c dec32be.c enc32be.c
  ec/ec_all_m31.c ec_c25519_m31.c ec_p256_m31.c ec_prime_i31.c ec_secp256r1.c ec_secp384r1.c
     ec_secp521r1.c ecdsa_atr.c ecdsa_i31_bits.c ecdsa_i31_vrfy_asn1.c ecdsa_i31_vrfy_raw.c
  hash/ghash_ctmul.c multihash.c sha2small.c
  int/i31_add.c i31_bitlen.c i31_decmod.c i31_decode.c i31_encode.c i31_fmont.c i31_iszero.c
      i31_modpow.c i31_modpow2.c i31_montmul.c i31_muladd.c i31_ninv31.c i31_rshift.c i31_sub.c
      i31_tmont.c i32_div32.c
  mac/hmac.c
  rand/hmac_drbg.c sysrng.c
  rsa/rsa_i31_pkcs1_vrfy.c rsa_i31_pub.c rsa_pkcs1_sig_unpad.c
  ssl/prf.c prf_sha256.c ssl_client.c ssl_engine.c ssl_engine_default_aesgcm.c ssl_hs_client.c
      ssl_io.c ssl_rec_gcm.c
  symcipher/aes_ct.c aes_ct_ctr.c aes_ct_enc.c
  x509/x509_decoder.c

使う暗号の組を増やすときは、リンクで足りないと言われたファイルを同じ元から足す。
