using TMPro;
using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Renders a flat 3D compass ring floating in front of the user.
    /// Cardinal labels (N/E/S/W) rotate with the ring so they always reflect
    /// the true geographic directions.
    /// </summary>
    public class CompassRingVisual : MonoBehaviour
    {
        [Header("Ring")]
        [SerializeField] private Transform     ringTransform;
        [SerializeField] private float         ringDistance = 1.8f;
        [SerializeField] private float         ringHeight   = -0.5f;

        [Header("Cardinal Labels")]
        [SerializeField] private TextMeshPro northLabel;
        [SerializeField] private TextMeshPro eastLabel;
        [SerializeField] private TextMeshPro southLabel;
        [SerializeField] private TextMeshPro westLabel;

        private CompassManager _compassMgr;

        private void Awake()
        {
            _compassMgr = FindObjectOfType<CompassManager>();
        }

        private void Update()
        {
            if (_compassMgr == null) return;

            // Rotate ring opposite to device heading so N always points north
            float heading = _compassMgr.SmoothedTrueHeading;

            if (ringTransform != null)
                ringTransform.localRotation = Quaternion.Euler(0f, -heading, 0f);

            // Place ring in front of the user at a fixed distance
            if (Camera.main != null)
            {
                Vector3 forward = Camera.main.transform.forward;
                forward.y = 0;
                forward.Normalize();

                transform.position = Camera.main.transform.position
                                   + forward * ringDistance
                                   + Vector3.up * ringHeight;
            }

            // Billboard each label toward the user
            BillboardLabel(northLabel);
            BillboardLabel(eastLabel);
            BillboardLabel(southLabel);
            BillboardLabel(westLabel);
        }

        private static void BillboardLabel(TextMeshPro label)
        {
            if (label == null || Camera.main == null) return;
            label.transform.LookAt(Camera.main.transform);
            label.transform.Rotate(0, 180f, 0);
        }
    }
}
