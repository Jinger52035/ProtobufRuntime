"""
clean_thirdparty.py
清理 ThirdParty/protobuf 目录，只保留 UBT 编译所需的文件。
白名单策略：只保留 .h / .cc / .cpp / .c / .inc / .proto 和 LICENSE 文件，其余全部删除。
每次升级 protobuf 后运行一次即可。

用法：
    python clean_thirdparty.py
"""

import os
import shutil
import re

BASE = os.path.join(os.path.dirname(__file__),
                    "Source", "ThirdParty", "protobuf")
BASE = os.path.normpath(BASE)

# ---------------------------------------------------------------------------
# 1. 删除整个目录（在逐文件扫描之前先批量删除，速度更快）
# ---------------------------------------------------------------------------

REMOVE_DIRS = [
    # 根目录下的非 C++ 语言绑定和无关目录
    ".git",
    ".github",
    ".bcr",
    "bazel",
    "benchmarks",
    "ci",
    "cmake",
    "compatibility",
    "conformance",
    "csharp",
    "docs",
    "editions",
    "editors",
    "examples",
    "go",
    "hpb",
    "hpb_generator",
    "java",
    "lua",
    "objectivec",
    "patches",
    "php",
    "pkg",
    "python",
    "ruby",
    "rust",
    "toolchain",
    "upb",
    "upb_generator",
    # src 内的非 C++ 语言代码生成器
    "src/google/protobuf/compiler/java",
    "src/google/protobuf/compiler/rust",
    "src/google/protobuf/compiler/objectivec",
    "src/google/protobuf/compiler/csharp",
    "src/google/protobuf/compiler/python",
    "src/google/protobuf/compiler/kotlin",
    "src/google/protobuf/compiler/ruby",
    "src/google/protobuf/compiler/php",
    # 测试数据与测试工具
    "src/google/protobuf/testdata",
    "src/google/protobuf/testing",
    # Linux/Solaris 平台兼容层
    "src/solaris",
    # abseil 时区测试数据
    "third_party/abseil-cpp/absl/time/internal/cctz/testdata",
    # abseil CI 脚本
    "third_party/abseil-cpp/ci",
    # utf8_range 模糊测试与附加工具
    "third_party/utf8_range/fuzz",
    "third_party/utf8_range/utf8_to_utf16",
]

# ---------------------------------------------------------------------------
# 2. 白名单：只保留这些扩展名的文件
# ---------------------------------------------------------------------------

KEEP_EXTENSIONS = {
    ".h", ".cc", ".cpp", ".c", ".inc", ".proto",
}

# 无扩展名但需要保留的文件名
KEEP_FILENAMES = {
    "LICENSE",
}

# ---------------------------------------------------------------------------
# 3. 测试 .proto 文件过滤（保留 well-known types，删除测试用 proto）
# ---------------------------------------------------------------------------

KEEP_PROTOS = {
    "any.proto", "api.proto", "cpp_features.proto", "descriptor.proto",
    "duration.proto", "empty.proto", "field_mask.proto",
    "source_context.proto", "struct.proto", "timestamp.proto",
    "type.proto", "wrappers.proto", "plugin.proto",
    "message_set.proto", "json_format.proto",
}

REMOVE_PROTO_PATTERNS = [
    re.compile(r"unittest"),
    re.compile(r"_test\.proto$"),
    re.compile(r"test_messages"),
    re.compile(r"test_bad"),
    re.compile(r"test_large"),
    re.compile(r"test_plugin"),
    re.compile(r"analyze_profile"),
    re.compile(r"edition_unittest"),
    re.compile(r"sample_messages"),
    re.compile(r"json_format_proto3"),
    re.compile(r"message_differencer_unittest"),
    re.compile(r"map_lite_unittest"),
    re.compile(r"map_proto[23]_unittest"),
    re.compile(r"map_unittest"),
]

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def force_rm_handler(func, path, _):
    """onerror handler：强制移除 Windows 只读文件（例如 .git 内容）"""
    os.chmod(path, 0o777)
    func(path)

def remove(path):
    if not os.path.exists(path):
        return
    if os.path.isdir(path):
        shutil.rmtree(path, onerror=force_rm_handler)
        print(f"  [dir]  {os.path.relpath(path, BASE)}")
    else:
        os.chmod(path, 0o777)
        os.remove(path)
        print(f"  [file] {os.path.relpath(path, BASE)}")

def should_keep(fname):
    """返回 True 表示保留该文件。"""
    ext = os.path.splitext(fname)[1].lower()

    # 始终保留白名单文件名（如 LICENSE）
    if fname in KEEP_FILENAMES:
        return True

    # 不在白名单扩展名内 → 删除
    if ext not in KEEP_EXTENSIONS:
        return False

    # .proto 文件额外过滤：well-known 保留，测试 proto 删除
    if ext == ".proto":
        if fname in KEEP_PROTOS:
            return True
        for pat in REMOVE_PROTO_PATTERNS:
            if pat.search(fname):
                return False
        return True  # 其余 .proto 保留

    return True  # .h / .cc / .cpp / .c / .inc 全部保留

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------

print(f"Cleaning (whitelist mode): {BASE}\n")

# Step 1: 删除整个目录
for d in REMOVE_DIRS:
    remove(os.path.join(BASE, d.replace("/", os.sep)))

# Step 2: 遍历剩余文件，删除不在白名单内的文件
for root, dirs, files in os.walk(BASE, topdown=True):
    for fname in files:
        if not should_keep(fname):
            remove(os.path.join(root, fname))

# Step 3: 删除因此产生的空目录（自底向上）
for root, dirs, files in os.walk(BASE, topdown=False):
    if root == BASE:
        continue
    try:
        os.rmdir(root)
        print(f"  [empty dir] {os.path.relpath(root, BASE)}")
    except OSError:
        pass  # 非空，保留

print(f"\nDone.")

import subprocess
result = subprocess.run(["du", "-sh", BASE], capture_output=True, text=True)
if result.stdout.strip():
    print(result.stdout.strip())
