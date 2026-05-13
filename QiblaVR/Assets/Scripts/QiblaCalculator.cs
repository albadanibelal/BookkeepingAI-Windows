using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Calculates the bearing angle from any geographic point to the Kaaba in Mecca.
    /// Uses the great-circle bearing formula for accuracy across long distances.
    /// </summary>
    public static class QiblaCalculator
    {
        // Kaaba coordinates (Mecca, Saudi Arabia)
        private const double KaabaLatitude  = 21.4225;
        private const double KaabaLongitude = 39.8262;

        /// <summary>
        /// Returns the Qibla bearing in degrees (0–360, clockwise from true North)
        /// for the given WGS-84 latitude and longitude.
        /// </summary>
        public static float GetQiblaBearing(double userLatDeg, double userLonDeg)
        {
            double lat1 = userLatDeg  * Mathf.Deg2Rad;
            double lon1 = userLonDeg  * Mathf.Deg2Rad;
            double lat2 = KaabaLatitude  * Mathf.Deg2Rad;
            double lon2 = KaabaLongitude * Mathf.Deg2Rad;

            double dLon = lon2 - lon1;

            double y = System.Math.Sin(dLon) * System.Math.Cos(lat2);
            double x = System.Math.Cos(lat1) * System.Math.Sin(lat2)
                     - System.Math.Sin(lat1) * System.Math.Cos(lat2) * System.Math.Cos(dLon);

            double bearing = System.Math.Atan2(y, x) * (180.0 / System.Math.PI);
            return (float)((bearing + 360.0) % 360.0);
        }

        /// <summary>
        /// Returns the great-circle distance in kilometres between the user and the Kaaba.
        /// </summary>
        public static float GetDistanceToMeccaKm(double userLatDeg, double userLonDeg)
        {
            const double EarthRadiusKm = 6371.0;

            double lat1 = userLatDeg      * Mathf.Deg2Rad;
            double lat2 = KaabaLatitude   * Mathf.Deg2Rad;
            double dLat = (KaabaLatitude  - userLatDeg)  * Mathf.Deg2Rad;
            double dLon = (KaabaLongitude - userLonDeg)  * Mathf.Deg2Rad;

            double a = System.Math.Sin(dLat / 2) * System.Math.Sin(dLat / 2)
                     + System.Math.Cos(lat1) * System.Math.Cos(lat2)
                     * System.Math.Sin(dLon / 2) * System.Math.Sin(dLon / 2);

            double c = 2 * System.Math.Atan2(System.Math.Sqrt(a), System.Math.Sqrt(1 - a));
            return (float)(EarthRadiusKm * c);
        }
    }
}
