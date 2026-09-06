# 1. 生成官方私钥 (private_key.pem) - 极其重要，绝对不能泄露！
openssl genpkey -algorithm RSA -out private_key.pem -pkeyopt rsa_keygen_bits:2048

# 2. 从私钥中提取公钥 (public_key.pem) - 这个文件要内置到你的 C++ 设备代码中
openssl rsa -pubout -in private_key.pem -out public_key.pem

# 3. 使用私钥对 manifest.json 进行 SHA-256 签名，生成 manifest.sig
openssl dgst -sha256 -sign private_key.pem -out manifest.sig manifest.json

# 4. 验证签名是否正确
openssl dgst -sha256 -verify public_key.pem -signature manifest.sig manifest.json

# 计算 update.bin 的 SHA-256 哈希值
sha256sum FILENAME

# manifest.json (明文信息)，manifest.sig (刚刚生成的签名文件)，实际的固件或程序文件 (app.bin 等)

openssl dgst -sha256 \
  -sign /home/xingxinliao/update/config/private_key.pem \
  -out /home/xingxinliao/update/config/manifest.sig \
  /home/xingxinliao/Downloads/db11_2026_09_01_dv11_manifest.json



  openssl dgst -sha256 \
  -verify /home/xingxinliao/update/config/public_key.pem \
  -signature /home/xingxinliao/update/config/manifest.sig \
  /home/xingxinliao/Downloads/db11_2026_09_01_dv11_manifest.json