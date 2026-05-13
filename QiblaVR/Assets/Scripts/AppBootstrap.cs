using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Entry point: wires cross-cutting concerns at startup.
    /// Place this on a persistent GameObject in the scene.
    /// </summary>
    public class AppBootstrap : MonoBehaviour
    {
        [SerializeField] private LocationManager          locationManager;
        [SerializeField] private MagneticDeclinationService declinationService;

        private void Start()
        {
            // Once we get a GPS fix, also fetch magnetic declination for true-north accuracy
            locationManager.OnLocationAcquired += OnFirstLocationFix;

            // Target 72 fps on Quest 3 for smooth tracking
            Application.targetFrameRate = 72;
            OVRManager.fixedFoveatedRenderingLevel = OVRManager.FixedFoveatedRenderingLevel.High;
        }

        private bool _declinationFetched;

        private void OnFirstLocationFix(double lat, double lon, float accuracy)
        {
            if (_declinationFetched) return;
            _declinationFetched = true;
            declinationService?.FetchDeclination(lat, lon);
        }

        private void OnDestroy()
        {
            if (locationManager != null)
                locationManager.OnLocationAcquired -= OnFirstLocationFix;
        }
    }
}
