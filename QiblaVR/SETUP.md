# QiblaVR — Meta Quest 3 Setup Guide

A mixed-reality VR app that shows the Qibla direction (toward the Kaaba in Mecca)
using GPS location and the device compass — inspired by Google's Qibla Finder.

---

## Prerequisites

| Tool | Version |
|------|---------|
| Unity Hub | Latest |
| Unity Editor | 2022.3 LTS (or 2023.x) |
| Meta Quest Developer Hub | Latest |
| Android Build Support module | Included in Unity install |
| Meta Developer Account | https://developer.oculus.com |

---

## 1. Open the Project

1. Open **Unity Hub → Add → Add project from disk**
2. Select the `QiblaVR/` folder
3. Unity will import packages from `Packages/manifest.json` automatically
   (this may take several minutes on first open)

---

## 2. Configure Meta XR SDK

1. Go to **Edit → Project Settings → XR Plug-in Management**
2. Select the **Android** tab
3. Check **Meta XR** (or **Oculus**)
4. Under **Edit → Project Settings → Meta XR** confirm:
   - **Target Devices**: Quest 3 checked
   - **Passthrough Support**: Supported

---

## 3. Build the Scene

1. Open **Assets/Scenes/QiblaMain.unity**  
   *— or —*  
   Run **QiblaVR → Build Scene** from the Unity menu bar to auto-generate it.

2. In the **Hierarchy**, wire up Inspector references:
   - **QiblaDirectionController**: assign `OVRCameraRig` → `Camera Rig`, `QiblaArrow` → `Arrow Visual`, `InfoCanvas` → `UI Controller`
   - **AppBootstrap**: assign `LocationManager` and `MagneticDeclinationService`
   - **PassthroughManager**: assign `OVRPassthroughLayer` and the center eye camera

---

## 4. Android Build Settings

1. **File → Build Settings → Android**
2. Click **Switch Platform**
3. Set:
   - **Texture Compression**: ASTC
   - **Target API Level**: 33
   - **Scripting Backend**: IL2CPP
   - **Target Architectures**: ARM64 only
4. **Player Settings**:
   - **Company Name / Product Name**: yours
   - **Bundle Identifier**: `com.yourcompany.qiblavr`
   - **Minimum API Level**: 32

---

## 5. Enable Developer Mode on Quest 3

1. Open the **Meta Quest mobile app** on your phone
2. Go to **Devices → your Quest 3 → Developer Mode → Enable**
3. Put on your headset and accept the developer mode prompt

---

## 6. Sideload & Test

```bash
# Connect Quest 3 via USB-C
adb devices          # should list your headset

# Build from Unity: File → Build Settings → Build and Run
# — or build the APK and install manually:
adb install -r QiblaVR.apk
```

---

## 7. Submit to Meta Horizon Store

1. Create an app at https://developer.oculus.com → **My Apps → Create New App**
2. Select **App Type: VR**; **Device: Quest 3**
3. Build a **Release AAB** in Unity (File → Build Settings → check "Build App Bundle")
4. Upload the AAB in the **Meta Developer Dashboard → Releases → Upload Build**
5. Fill in:
   - Store listing (title, description, screenshots, trailer)
   - Age rating (IARC questionnaire)
   - Privacy policy URL
   - Data safety form
6. Submit for review (~5–10 business days)

---

## Architecture Overview

```
AppBootstrap
 ├── LocationManager        — GPS via Unity Input.location
 │    └─ fires OnLocationAcquired
 ├── MagneticDeclinationService — fetches NOAA declination correction
 └── QiblaDirectionController
      ├── QiblaCalculator   — great-circle bearing math (static)
      ├── CompassManager    — smoothed device heading
      ├── QiblaArrowVisual  — 3D glowing arrow in VR space
      ├── CompassRingVisual — floating N/E/S/W ring
      └── UIController      — world-space info panel (TextMeshPro)

PassthroughManager          — Quest 3 MR passthrough (see-through)
```

---

## Key Scripts

| File | Purpose |
|------|---------|
| `QiblaCalculator.cs` | Great-circle bearing & distance to Mecca |
| `LocationManager.cs` | GPS acquisition & polling |
| `CompassManager.cs` | Smoothed magnetic/IMU heading |
| `QiblaDirectionController.cs` | Ties location + compass → arrow position |
| `QiblaArrowVisual.cs` | 3D arrow glow, pulse, alignment feedback |
| `CompassRingVisual.cs` | Floating compass ring with cardinal labels |
| `PassthroughManager.cs` | Meta Quest 3 mixed-reality passthrough |
| `UIController.cs` | World-space HUD (bearing, distance, accuracy) |
| `MagneticDeclinationService.cs` | NOAA API for true-north correction |
| `SceneSetup.cs` | Editor utility to build the scene automatically |
