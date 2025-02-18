import hashlib
from urllib.parse import unquote

def IAM_FUN_EncryptString(IAM_VAR_EncryptString_serviceName, IAM_VAR_EncryptString_servicePasswd, IAM_VAR_EncryptString_timeStamp):
    # 对时间戳进行URL解码
    IAM_VAR_EncryptString_timeStamp_decoded = unquote(IAM_VAR_EncryptString_timeStamp)
    
    # 将三个字符串用加号连接
    IAM_VAR_EncryptString_input = f"{IAM_VAR_EncryptString_serviceName}+{IAM_VAR_EncryptString_servicePasswd}+{IAM_VAR_EncryptString_timeStamp_decoded}"
    
    # 进行SHA-256加密
    hash = hashlib.sha256(IAM_VAR_EncryptString_input.encode('utf-8')).digest()
    
    # 将哈希值转换为十六进制字符串
    IAM_VAR_EncryptString_output = ''.join(f"{byte:02x}" for byte in hash)
    
    return IAM_VAR_EncryptString_output

EncryptString_serviceName = input("用户名: ")
EncryptString_servicePasswd = input("用户密码: ")
EncryptString_timeStamp = input("时间戳: ")
'''
EncryptString_serviceName = "myService"
EncryptString_servicePasswd = "mySecretPassword"
EncryptString_timeStamp = "2023-12-12%2010:39:07"
'''

token = IAM_FUN_EncryptString(EncryptString_serviceName, EncryptString_servicePasswd, EncryptString_timeStamp)
print(token)