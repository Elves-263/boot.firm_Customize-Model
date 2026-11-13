BearSSL 0.6（https://bearssl.org/、MIT ライセンス、common/bearssl/LICENSE.txt）のうち、
更新アプリの HTTPS（https.c。br_ssl_client_init_full と br_x509_minimal）だけが使うファイルを、
手を加えずに写したもの。Rosalina と共通の分とヘッダーは common/bearssl/ にあり、両方を合わせて使う。

元: common/bearssl/README.txt と同じ（bearssl_0.6+dfsg.1.orig.tar.xz）。src/ の下の次のファイルを、
フォルダを分けずに置いた。
  aead/ccm.c
  codec/dec32le.c dec64be.c enc32le.c enc64be.c
  hash/md5.c sha1.c sha2big.c
  mac/hmac_ct.c
  rsa/rsa_default_pkcs1_vrfy.c rsa_default_pub.c
  ssl/prf_md5sha1.c prf_sha384.c ssl_client_default_rsapub.c ssl_client_full.c
      ssl_engine_default_aescbc.c ssl_engine_default_aesccm.c ssl_engine_default_chapol.c
      ssl_engine_default_descbc.c ssl_engine_default_ecdsa.c ssl_engine_default_rsavrfy.c
      ssl_rec_cbc.c ssl_rec_ccm.c ssl_rec_chapol.c
  symcipher/aes_ct_cbcdec.c aes_ct_cbcenc.c aes_ct_ctrcbc.c aes_ct_dec.c chacha20_ct.c des_ct.c
      des_ct_cbcdec.c des_ct_cbcenc.c des_support.c poly1305_ctmul.c
  x509/x509_minimal.c
