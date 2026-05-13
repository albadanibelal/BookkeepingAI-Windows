using System;
using System.Collections;
using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Requests and maintains the device GPS location.
    /// Raises events when a fix is acquired or an error occurs.
    /// </summary>
    public class LocationManager : MonoBehaviour
    {
        [Header("GPS Settings")]
        [SerializeField] private float desiredAccuracyMeters = 10f;
        [SerializeField] private float updateDistanceMeters  = 5f;
        [SerializeField] private float locationTimeoutSec    = 20f;

        public event Action<double, double, float> OnLocationAcquired; // lat, lon, accuracy
        public event Action<string>                OnLocationError;

        public bool HasFix         { get; private set; }
        public double Latitude     { get; private set; }
        public double Longitude    { get; private set; }
        public float  AccuracyM    { get; private set; }

        private Coroutine _locationCoroutine;

        private void OnEnable()
        {
            _locationCoroutine = StartCoroutine(StartLocationServices());
        }

        private void OnDisable()
        {
            if (_locationCoroutine != null)
                StopCoroutine(_locationCoroutine);

            if (Input.location.status == LocationServiceStatus.Running)
                Input.location.Stop();
        }

        private IEnumerator StartLocationServices()
        {
            // Android/Quest requires the user to enable location in the system.
            if (!Input.location.isEnabledByUser)
            {
                OnLocationError?.Invoke("Location services are disabled. Please enable them in device settings.");
                yield break;
            }

            Input.location.Start(desiredAccuracyMeters, updateDistanceMeters);
            Input.compass.enabled = true;

            float elapsed = 0f;
            while (Input.location.status == LocationServiceStatus.Initializing && elapsed < locationTimeoutSec)
            {
                elapsed += Time.deltaTime;
                yield return null;
            }

            if (elapsed >= locationTimeoutSec)
            {
                OnLocationError?.Invoke("GPS timed out. Please check device location settings.");
                yield break;
            }

            if (Input.location.status == LocationServiceStatus.Failed)
            {
                OnLocationError?.Invoke("GPS unavailable on this device.");
                yield break;
            }

            // Initial fix
            UpdateFromService();

            // Keep polling for better accuracy
            while (true)
            {
                UpdateFromService();
                yield return new WaitForSeconds(2f);
            }
        }

        private void UpdateFromService()
        {
            var data = Input.location.lastData;
            Latitude   = data.latitude;
            Longitude  = data.longitude;
            AccuracyM  = data.horizontalAccuracy;
            HasFix     = true;
            OnLocationAcquired?.Invoke(Latitude, Longitude, AccuracyM);
        }
    }
}
