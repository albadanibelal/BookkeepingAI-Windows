using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Provides a smoothed magnetic heading from the device compass.
    /// On Meta Quest 3 the headset's IMU heading is used as primary source;
    /// the Unity Input.compass serves as a fallback for non-VR testing.
    /// </summary>
    public class CompassManager : MonoBehaviour
    {
        [Header("Smoothing")]
        [SerializeField, Range(1f, 30f)] private float smoothingSpeed = 10f;

        // Declination correction fetched from NOAA model (set via Inspector or runtime API)
        [Header("Magnetic Declination")]
        [SerializeField] private float magneticDeclinationDeg = 0f;

        private float _rawHeading;
        private float _smoothedHeading;

        public float SmoothedTrueHeading => _smoothedHeading;

        private void Start()
        {
            Input.compass.enabled = true;
        }

        private void Update()
        {
            // Meta Quest 3 exposes yaw via OVRManager.display or the camera rig.
            // We fall back to Input.compass for editor/non-VR builds.
#if UNITY_ANDROID && !UNITY_EDITOR
            // OVRInput gives us the headset's world-space yaw relative to its
            // tracking reference frame, which is aligned to magnetic north on Quest.
            var rotation = OVRInput.GetLocalControllerRotation(OVRInput.Controller.Head);
            _rawHeading  = rotation.eulerAngles.y;
#else
            _rawHeading = Input.compass.trueHeading;
#endif
            // Apply magnetic declination to get true north bearing
            float corrected = (_rawHeading + magneticDeclinationDeg + 360f) % 360f;

            // Smooth to avoid jitter
            _smoothedHeading = Mathf.LerpAngle(_smoothedHeading, corrected, smoothingSpeed * Time.deltaTime);
        }

        /// <summary>Set the magnetic declination for the user's location (degrees East positive).</summary>
        public void SetMagneticDeclination(float declinationDeg)
        {
            magneticDeclinationDeg = declinationDeg;
        }
    }
}
