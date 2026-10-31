# Sao chép code PlatformIO sang sketch Arduino IDE:
#   src/uno + include/protocol.h -> arduino/uno/      (ATmega328P)
#   src/esp + include/protocol.h -> arduino/esp8266/  (ESP8266)
# PlatformIO tự chạy script này trước mỗi lần build (extra_scripts trong platformio.ini),
# nên sketch Arduino IDE luôn trùng với code PlatformIO. Chạy tay: python tools/sync_arduino.py
#
# Nguồn gốc là src/: sửa code ở đó, đừng sửa trực tiếp trong arduino/ (sẽ bị ghi đè).
import os
import shutil

try:
    Import("env")  # noqa: F821  (chạy trong PlatformIO)
    ROOT = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (thư mục nguồn trong src/, thư mục sketch trong arduino/); main.cpp được đổi tên
# thành <sketch>_main.cpp để không trùng main.cpp của lõi Arduino
TARGETS = [("uno", "uno"), ("esp", "esp8266")]


def sync(src_name, sketch):
    src_dir = os.path.join(ROOT, "src", src_name)
    dst_dir = os.path.join(ROOT, "arduino", sketch)
    files = [(os.path.join(src_dir, f), sketch + "_main.cpp" if f == "main.cpp" else f)
             for f in sorted(os.listdir(src_dir)) if f.endswith((".cpp", ".h"))]
    files.append((os.path.join(ROOT, "include", "protocol.h"), "protocol.h"))

    os.makedirs(dst_dir, exist_ok=True)
    for src, name in files:
        dst = os.path.join(dst_dir, name)
        with open(src, "rb") as f:
            data = f.read()
        if os.path.exists(dst):
            with open(dst, "rb") as f:
                if f.read() == data:
                    continue
        shutil.copyfile(src, dst)
        print("sync_arduino: updated arduino/%s/%s" % (sketch, name))


for src_name, sketch in TARGETS:
    sync(src_name, sketch)
