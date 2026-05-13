#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace QiblaVR.Editor
{
    /// <summary>
    /// Editor utility: run this once from the menu to build the main scene
    /// with all required GameObjects pre-configured.
    /// Menu: QiblaVR → Build Scene
    /// </summary>
    public static class SceneSetup
    {
        [MenuItem("QiblaVR/Build Scene")]
        public static void BuildScene()
        {
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);

            // ── OVR Camera Rig ──────────────────────────────────────────────────
            var ovrRigPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(
                "Packages/com.meta.xr.sdk.core/Prefabs/OVRCameraRig.prefab");

            GameObject cameraRig = ovrRigPrefab != null
                ? (GameObject)PrefabUtility.InstantiatePrefab(ovrRigPrefab)
                : new GameObject("OVRCameraRig");

            cameraRig.name = "OVRCameraRig";

            // ── OVR Manager ─────────────────────────────────────────────────────
            var ovrManagerGO = new GameObject("OVRManager");
            var ovrManager   = ovrManagerGO.AddComponent<OVRManager>();
            ovrManager.trackingOriginType             = OVRManager.TrackingOrigin.FloorLevel;
            ovrManager.isInsightPassthroughEnabled    = true;

            // ── Passthrough Layer ───────────────────────────────────────────────
            var passthroughGO    = new GameObject("PassthroughLayer");
            var passthroughLayer = passthroughGO.AddComponent<OVRPassthroughLayer>();
            passthroughLayer.projectionSurfaceType = OVRPassthroughLayer.ProjectionSurfaceType.Reconstructed;

            var passthroughMgr = passthroughGO.AddComponent<PassthroughManager>();

            // ── Core Managers ───────────────────────────────────────────────────
            var managersGO = new GameObject("Managers");
            managersGO.AddComponent<LocationManager>();
            managersGO.AddComponent<CompassManager>();
            var declSvc = managersGO.AddComponent<MagneticDeclinationService>();
            var bootstrap = managersGO.AddComponent<AppBootstrap>();

            // ── Qibla Controller ────────────────────────────────────────────────
            var controllerGO = new GameObject("QiblaDirectionController");
            controllerGO.AddComponent<LocationManager>();
            controllerGO.AddComponent<CompassManager>();
            var dirCtrl = controllerGO.AddComponent<QiblaDirectionController>();

            // ── Arrow Prefab Placeholder ─────────────────────────────────────────
            var arrowRoot = new GameObject("QiblaArrow");
            arrowRoot.AddComponent<QiblaArrowVisual>();
            arrowRoot.transform.position = new Vector3(0, 1.5f, 2.5f);

            // Arrow body
            var arrowBody = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            arrowBody.name = "ArrowBody";
            arrowBody.transform.SetParent(arrowRoot.transform);
            arrowBody.transform.localPosition = new Vector3(0, 0, 0);
            arrowBody.transform.localScale    = new Vector3(0.08f, 0.4f, 0.08f);
            arrowBody.transform.localRotation = Quaternion.Euler(90, 0, 0);

            // Arrow tip
            var arrowTip = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            arrowTip.name = "ArrowTip";
            arrowTip.transform.SetParent(arrowRoot.transform);
            arrowTip.transform.localPosition = new Vector3(0, 0, 0.42f);
            arrowTip.transform.localScale    = new Vector3(0.15f, 0.15f, 0.15f);

            // ── Compass Ring ─────────────────────────────────────────────────────
            var compassRingGO = new GameObject("CompassRing");
            compassRingGO.AddComponent<CompassRingVisual>();

            // Outer ring (torus approximation with cylinder)
            var ring = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            ring.name = "Ring";
            ring.transform.SetParent(compassRingGO.transform);
            ring.transform.localPosition = Vector3.zero;
            ring.transform.localScale    = new Vector3(1.0f, 0.01f, 1.0f);

            // ── World-Space UI Canvas ────────────────────────────────────────────
            var canvasGO  = new GameObject("InfoCanvas");
            var canvas    = canvasGO.AddComponent<Canvas>();
            canvas.renderMode     = RenderMode.WorldSpace;
            canvas.worldCamera    = Camera.main;
            canvasGO.transform.position = new Vector3(0, 1.8f, 2f);
            canvasGO.transform.localScale = new Vector3(0.003f, 0.003f, 0.003f);

            var uiCtrl = canvasGO.AddComponent<UIController>();

            // Save scene
            EditorSceneManager.SaveScene(scene, "Assets/Scenes/QiblaMain.unity");
            Debug.Log("[QiblaVR] Scene built and saved to Assets/Scenes/QiblaMain.unity");
        }
    }
}
#endif
