using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Enables Meta Quest 3 mixed-reality passthrough so the user sees the real
    /// world with the Qibla arrow overlaid — similar to an AR compass experience.
    /// Requires OVRManager in the scene with Passthrough Support set to "Supported" or "Required".
    /// </summary>
    [RequireComponent(typeof(OVRPassthroughLayer))]
    public class PassthroughManager : MonoBehaviour
    {
        [SerializeField] private OVRPassthroughLayer passthroughLayer;
        [SerializeField] private Camera             vrCamera;

        private void Awake()
        {
            if (passthroughLayer == null)
                passthroughLayer = GetComponent<OVRPassthroughLayer>();
        }

        private void Start()
        {
            EnablePassthrough();
        }

        private void EnablePassthrough()
        {
            // Make the camera background transparent so passthrough shows through
            if (vrCamera != null)
            {
                vrCamera.clearFlags       = CameraClearFlags.SolidColor;
                vrCamera.backgroundColor  = Color.clear;
            }

            // Tell the passthrough layer to render the real-world feed
            if (passthroughLayer != null)
                passthroughLayer.hidden = false;

            // OVRManager.instance controls the display-level passthrough
            if (OVRManager.instance != null)
            {
                OVRManager.instance.isInsightPassthroughEnabled = true;
            }
        }

        /// <summary>Toggle between full VR (dark skybox) and passthrough MR.</summary>
        public void TogglePassthrough(bool enabled)
        {
            if (passthroughLayer != null)
                passthroughLayer.hidden = !enabled;

            if (vrCamera != null)
            {
                vrCamera.clearFlags     = enabled ? CameraClearFlags.SolidColor : CameraClearFlags.Skybox;
                vrCamera.backgroundColor = enabled ? Color.clear : Color.black;
            }
        }
    }
}
