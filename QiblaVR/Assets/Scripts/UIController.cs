using TMPro;
using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Manages the world-space UI panel shown in the VR environment.
    /// Displays location accuracy, Qibla bearing, and distance to Mecca.
    /// </summary>
    public class UIController : MonoBehaviour
    {
        [Header("Status Panel")]
        [SerializeField] private TextMeshProUGUI statusLabel;
        [SerializeField] private TextMeshProUGUI bearingLabel;
        [SerializeField] private TextMeshProUGUI distanceLabel;
        [SerializeField] private TextMeshProUGUI accuracyLabel;
        [SerializeField] private TextMeshProUGUI errorLabel;

        [Header("Loading Indicator")]
        [SerializeField] private GameObject loadingSpinner;

        private void Start()
        {
            SetLoadingState(true);
            HideError();
        }

        public void UpdateInfo(double lat, double lon, float accuracyM, float qiblaBearing, float distKm)
        {
            SetLoadingState(false);
            HideError();

            if (statusLabel  != null) statusLabel.text  = $"Location: {lat:F4}°, {lon:F4}°";
            if (bearingLabel != null) bearingLabel.text  = $"Qibla: {qiblaBearing:F1}°";
            if (distanceLabel!= null) distanceLabel.text = $"Distance to Mecca: {distKm:F0} km";
            if (accuracyLabel!= null) accuracyLabel.text = $"GPS Accuracy: ±{accuracyM:F0} m";
        }

        public void ShowError(string message)
        {
            SetLoadingState(false);
            if (errorLabel != null)
            {
                errorLabel.gameObject.SetActive(true);
                errorLabel.text = message;
            }
        }

        private void HideError()
        {
            if (errorLabel != null)
                errorLabel.gameObject.SetActive(false);
        }

        private void SetLoadingState(bool isLoading)
        {
            if (loadingSpinner != null)
                loadingSpinner.SetActive(isLoading);

            if (statusLabel != null && isLoading)
                statusLabel.text = "Acquiring GPS location…";
        }
    }
}
