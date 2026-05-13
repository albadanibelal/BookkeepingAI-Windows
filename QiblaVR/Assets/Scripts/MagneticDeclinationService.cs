using System;
using System.Collections;
using UnityEngine;
using UnityEngine.Networking;

namespace QiblaVR
{
    /// <summary>
    /// Fetches magnetic declination from the NOAA World Magnetic Model API so the
    /// compass heading can be corrected to true north.
    ///
    /// API docs: https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml
    /// </summary>
    public class MagneticDeclinationService : MonoBehaviour
    {
        private const string NoaaApiUrl =
            "https://www.ngdc.noaa.gov/geomag-web/calculators/calculateDeclination" +
            "?lat1={0}&lon1={1}&resultFormat=json";

        [SerializeField] private CompassManager compassManager;

        public void FetchDeclination(double lat, double lon)
        {
            StartCoroutine(FetchDeclinationCoroutine(lat, lon));
        }

        private IEnumerator FetchDeclinationCoroutine(double lat, double lon)
        {
            string url = string.Format(NoaaApiUrl, lat.ToString("F4"), lon.ToString("F4"));

            using var request = UnityWebRequest.Get(url);
            request.timeout = 10;
            yield return request.SendWebRequest();

            if (request.result != UnityWebRequest.Result.Success)
            {
                Debug.LogWarning($"[DeclinationService] Request failed: {request.error}");
                yield break;
            }

            float declination = ParseDeclination(request.downloadHandler.text);
            if (compassManager != null)
                compassManager.SetMagneticDeclination(declination);

            Debug.Log($"[DeclinationService] Magnetic declination: {declination:F2}°");
        }

        private static float ParseDeclination(string json)
        {
            // Minimal JSON parse — avoids a full JSON library dependency.
            // Response contains: "declination": <value>
            const string key = "\"declination\":";
            int idx = json.IndexOf(key, StringComparison.Ordinal);
            if (idx < 0) return 0f;

            int start = idx + key.Length;
            int end   = json.IndexOfAny(new[] { ',', '}' }, start);
            if (end < 0) return 0f;

            string raw = json.Substring(start, end - start).Trim();
            return float.TryParse(raw, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out float val) ? val : 0f;
        }
    }
}
