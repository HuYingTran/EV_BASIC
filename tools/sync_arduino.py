# Sao chép code ATmega328P (src/uno + include/protocol.h) sang sketch Arduino IDE (arduino/uno/).
# PlatformIO tự chạy script này trước mỗi lần build env uno (extra_scripts trong platformio.ini),
# nên sketch Arduino IDE luôn trùng với code PlatformIO. Chạy tay: python tools/sync_arduino.py
#
# Nguồn gốc là src/uno: sửa code ở đó, đừng sửa trực tiếp trong arduino/uno (sẽ bị ghi đè).
import os
import shutil

try:
    Import("env")  # noqa: F821  (chạy trong PlatformIO)
    ROOT = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC = os.path.join(ROOT, "src", "uno")
DST = os.path.join(ROOT, "arduino", "uno")
# main.cpp đổi tên để không trùng main.cpp của lõi Arduino
RENAME = {"main.cpp": "uno_main.cpp"}

files = [(os.path.join(SRC, f), RENAME.get(f, f)) for f in sorted(os.listdir(SRC))
         if f.endswith((".cpp", ".h"))]
files.append((os.path.join(ROOT, "include", "protocol.h"), "protocol.h"))

os.makedirs(DST, exist_ok=True)
for src, name in files:
    dst = os.path.join(DST, name)
    with open(src, "rb") as f:
        data = f.read()
    if os.path.exists(dst):
        with open(dst, "rb") as f:
            if f.read() == data:
                continue
    shutil.copyfile(src, dst)
    print("sync_arduino: updated arduino/uno/" + name)
