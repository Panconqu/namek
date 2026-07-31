import uuid
import hashlib
import urllib.request
import urllib.parse
import urllib.error
import json as _json
import base64
from typing import Dict, Any, Optional

class UUID:
    @staticmethod
    def v4() -> str:
        return str(uuid.uuid4())

class Hash:
    @staticmethod
    def sha256(text: str) -> str:
        return hashlib.sha256(text.encode("utf-8")).hexdigest()

    @staticmethod
    def md5(text: str) -> str:
        return hashlib.md5(text.encode("utf-8")).hexdigest()

class Crypto:
    @staticmethod
    def base64_encode(text: str) -> str:
        return base64.b64encode(text.encode("utf-8")).decode("utf-8")

    @staticmethod
    def base64_decode(text: str) -> str:
        return base64.b64decode(text.encode("utf-8")).decode("utf-8")

    @staticmethod
    def xor(text: str, key: int = 0x3F) -> str:
        return "".join(chr(ord(c) ^ key) for c in text)

class HTTP:
    @staticmethod
    def request(method: str, url: str, data: Any = None,
                headers: Optional[Dict[str, str]] = None,
                timeout: int = 10) -> Dict[str, Any]:
        req = urllib.request.Request(url, data=data, headers=headers or {},
                                     method=method.upper())
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                body = resp.read().decode("utf-8", errors="replace")
                return {
                    "status": resp.status,
                    "headers": dict(resp.headers.items()),
                    "body": body,
                    "ok": 200 <= resp.status < 300,
                }
        except urllib.error.HTTPError as e:
            return {
                "status": e.code,
                "headers": dict(e.headers.items()),
                "body": e.read().decode("utf-8", errors="replace"),
                "ok": False,
            }
        except Exception as e:
            return {"status": 0, "headers": {}, "body": "", "ok": False, "error": str(e)}

    @staticmethod
    def get(url: str, headers: Optional[Dict[str, str]] = None,
            timeout: int = 10) -> Dict[str, Any]:
        return HTTP.request("GET", url, headers=headers, timeout=timeout)

    @staticmethod
    def post(url: str, data: Any = None, json: Optional[Dict[str, Any]] = None,
             headers: Optional[Dict[str, str]] = None,
             timeout: int = 10) -> Dict[str, Any]:
        body = data
        hdrs = dict(headers or {})
        if json is not None:
            body = _json.dumps(json).encode("utf-8")
            hdrs.setdefault("Content-Type", "application/json")
        elif isinstance(data, dict):
            body = urllib.parse.urlencode(data).encode("utf-8")
            hdrs.setdefault("Content-Type", "application/x-www-form-urlencoded")
        return HTTP.request("POST", url, data=body, headers=hdrs, timeout=timeout)

class Obfuscator:
    @staticmethod
    def obfuscate_mv(source_code: str, key: int = 0x3F) -> str:
        bytecode = []
        for c in source_code:
            bytecode.extend([0x10, ord(c) ^ key])
        bytecode.append(0x99)
        
        return f"""# Namek Python MV Obfuscated Runtime
_opcodes = {bytecode}
_pc, _state = 0, []
while _pc < len(_opcodes):
    op = _opcodes[_pc]
    if op == 0x10:
        _state.append(chr(_opcodes[_pc + 1] ^ {key}))
        _pc += 2
    elif op == 0x99:
        exec(''.join(_state))
        break
    else: _pc += 1
"""

    @staticmethod
    def deobfuscate_mv(obf_code: str, key: int = 0x3F) -> str:
        try:
            start = obf_code.find("[")
            end = obf_code.find("]", start)
            opcodes = _json.loads(obf_code[start:end+1])
            decompiled = []
            i = 0
            while i < len(opcodes):
                if opcodes[i] == 0x10 and i + 1 < len(opcodes):
                    decompiled.append(chr(opcodes[i+1] ^ key))
                    i += 2
                else:
                    i += 1
            return "".join(decompiled)
        except Exception as e:
            return f"# Deobfuscation error: {e}"

class TextUtils:
    @staticmethod
    def slugify(text: str) -> str:
        return "-".join(text.lower().strip().split())

    @staticmethod
    def truncate(text: str, max_length: int = 50) -> str:
        if len(text) <= max_length:
            return text
        return text[:max_length-3] + "..."
