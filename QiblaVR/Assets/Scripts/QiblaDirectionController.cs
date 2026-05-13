using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Central controller: wires together the LocationManager, CompassManager,
    /// and QiblaArrowVisual to keep the Qibla arrow pointing toward Mecca.
    /// </summary>
    [RequireComponent(typeof(LocationManager))]
    [RequireComponent(typeof(CompassManager))]
    public class QiblaDirectionController : MonoBehaviour
    {
        [Header("References")]
        [SerializeField] private QiblaArrowVisual arrowVisual;
        [SerializeField] private UIController     uiController;

        [Header("Arrow Placement")]
        [SerializeField] private Transform cameraRig;
        [SerializeField] private float     arrowDistance = 2.5f;
        [SerializeField] private float     arrowHeight   = -0.3f; // relative to eye level

        private LocationManager _locationMgr;
        private CompassManager  _compassMgr;

        private float _qiblaBearing = 0f;
        private bool  _hasLocation  = false;

        private void Awake()
        {
            _locationMgr = GetComponent<LocationManager>();
            _compassMgr  = GetComponent<CompassManager>();
        }

        private void OnEnable()
        {
            _locationMgr.OnLocationAcquired += HandleLocationAcquired;
            _locationMgr.OnLocationError    += HandleLocationError;
        }

        private void OnDisable()
        {
            _locationMgr.OnLocationAcquired -= HandleLocationAcquired;
            _locationMgr.OnLocationError    -= HandleLocationError;
        }

        private void Update()
        {
            if (!_hasLocation) return;

            // Device heading (degrees CW from true North)
            float deviceHeading = _compassMgr.SmoothedTrueHeading;

            // How many degrees the Qibla is offset from current heading
            float relativeAngle = (_qiblaBearing - deviceHeading + 360f) % 360f;

            // Place arrow in front of the camera, rotated to Qibla
            if (cameraRig != null)
                PositionArrow(relativeAngle);

            arrowVisual?.SetRelativeAngle(relativeAngle);
        }

        private void PositionArrow(float relativeAngleDeg)
        {
            Vector3 cameraPos = cameraRig.position;
            float   radians   = relativeAngleDeg * Mathf.Deg2Rad;

            // Build position on a horizontal circle around the camera
            Vector3 offset = new Vector3(
                Mathf.Sin(radians) * arrowDistance,
                arrowHeight,
                Mathf.Cos(radians) * arrowDistance
            );

            if (arrowVisual != null)
            {
                arrowVisual.transform.position = cameraPos + offset;
                arrowVisual.transform.LookAt(cameraPos + new Vector3(0, arrowHeight, 0));
                arrowVisual.transform.Rotate(0, 180f, 0); // face toward camera
            }
        }

        private void HandleLocationAcquired(double lat, double lon, float accuracy)
        {
            _qiblaBearing = QiblaCalculator.GetQiblaBearing(lat, lon);
            float distKm  = QiblaCalculator.GetDistanceToMeccaKm(lat, lon);
            _hasLocation  = true;

            arrowVisual?.SetVisible(true);
            uiController?.UpdateInfo(lat, lon, accuracy, _qiblaBearing, distKm);
        }

        private void HandleLocationError(string errorMessage)
        {
            uiController?.ShowError(errorMessage);
        }
    }
}
