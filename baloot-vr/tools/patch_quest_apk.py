#!/usr/bin/env python3
"""Turn a Godot non-Gradle Android export into a Meta Quest (Horizon OS) VR APK.

Godot only emits the OpenXR/Quest manifest entries in Gradle builds. This script
edits an apktool-decoded APK directory in place:
  * switches the baked launch flags to OpenXR mode
  * adds the Quest VR intent category, supported devices, head-tracking feature
    and the Khronos OpenXR loader broker queries
  * copies in the OpenXR loader native library
  * raises minSdkVersion to 29 as required by Horizon OS
"""
import re
import shutil
import struct
import sys
from pathlib import Path

dec = Path(sys.argv[1])
loader = Path(sys.argv[2])
package = sys.argv[3]

# 1. Launch flags (assets/_cl_: int32 count, then [int32 len, utf-8 bytes]...)
cl = dec / "assets" / "_cl_"
data = cl.read_bytes()
count = struct.unpack("<i", data[:4])[0]
off, args = 4, []
for _ in range(count):
    n = struct.unpack("<i", data[off:off + 4])[0]
    args.append(data[off + 4:off + 4 + n].decode())
    off += 4 + n
args = [a for a in args if a not in ("--xr_mode_regular", "--xr_mode_openxr")]
if "--xr-mode" in args:
    i = args.index("--xr-mode")
    del args[i:i + 2]
args = ["--xr_mode_openxr", "--xr-mode", "on"] + [a for a in args if a != "--fullscreen"]
out = struct.pack("<i", len(args))
for a in args:
    b = a.encode()
    out += struct.pack("<i", len(b)) + b
cl.write_bytes(out)

# 2. Manifest
mf = dec / "AndroidManifest.xml"
m = mf.read_text()
if "com.oculus.intent.category.VR" not in m:
    m = m.replace(
        '<category android:name="android.intent.category.LAUNCHER"/>',
        '<category android:name="android.intent.category.LAUNCHER"/>\n'
        '                <category android:name="com.oculus.intent.category.VR"/>')
    m = m.replace(
        '<uses-feature android:glEsVersion="0x00030000" android:required="true"/>',
        '<uses-feature android:glEsVersion="0x00030000" android:required="true"/>\n'
        '    <uses-feature android:name="android.hardware.vr.headtracking" android:required="true" android:version="1"/>\n'
        '    <uses-feature android:name="oculus.software.handtracking" android:required="false"/>\n'
        '    <uses-permission android:name="org.khronos.openxr.permission.OPENXR"/>\n'
        '    <uses-permission android:name="org.khronos.openxr.permission.OPENXR_SYSTEM"/>\n'
        '    <queries>\n'
        '        <provider android:authorities="org.khronos.openxr.runtime_broker;org.khronos.openxr.system_runtime_broker"/>\n'
        '        <intent><action android:name="org.khronos.openxr.OpenXRRuntimeService"/></intent>\n'
        '        <intent><action android:name="org.khronos.openxr.OpenXRApiLayerService"/></intent>\n'
        '    </queries>')
    m = m.replace(
        '<meta-data android:name="org.godotengine.editor.version"',
        '<meta-data android:name="com.oculus.supportedDevices" android:value="quest2|questpro|quest3|quest3s"/>\n'
        '        <meta-data android:name="com.oculus.vr.focusaware" android:value="true"/>\n'
        '        <meta-data android:name="org.godotengine.editor.version"')
    # Godot's naive package rename gives the androidx-startup provider the same authority as the file provider.
    m = re.sub(r'android:authorities="[^"]*" (android:exported="false" android:name="androidx.startup.InitializationProvider")',
               f'android:authorities="{package}.androidx-startup" \\1', m)
    m = m.replace('android:resizeableActivity="true"', 'android:resizeableActivity="false"')
    m = m.replace('android:screenOrientation="landscape"', 'android:screenOrientation="landscape"')
mf.write_text(m)

# 3. apktool decodes Godot's empty "themed_icon" placeholder as an invalid literal; point it at the icon.
for f in dec.glob("res/values*/mipmaps.xml"):
    f.write_text(f.read_text().replace('name="themed_icon">false<', 'name="themed_icon">@mipmap/icon<'))

# 4. OpenXR loader
dst = dec / "lib" / "arm64-v8a" / "libopenxr_loader.so"
shutil.copy(loader, dst)

# 5. SDK levels
yml = dec / "apktool.yml"
y = yml.read_text()
y = re.sub(r"minSdkVersion: \d+", "minSdkVersion: 29", y)
yml.write_text(y)
print("patched:", args)
