using UnityEngine;

namespace QiblaVR
{
    /// <summary>
    /// Controls the 3D Qibla arrow visual: glow, pulsing, and alignment indicator.
    /// Attach to the root GameObject of the arrow prefab.
    /// </summary>
    public class QiblaArrowVisual : MonoBehaviour
    {
        [Header("Arrow Parts")]
        [SerializeField] private Transform   arrowMesh;
        [SerializeField] private Transform   kaabaIconTransform;
        [SerializeField] private LineRenderer directionLine;

        [Header("Alignment Feedback")]
        [SerializeField] private Renderer    glowRenderer;
        [SerializeField] private Color       alignedColor   = new Color(0.0f, 0.9f, 0.4f);
        [SerializeField] private Color       misalignedColor = new Color(1.0f, 0.6f, 0.0f);
        [SerializeField] private float       alignedThresholdDeg = 5f;

        [Header("Pulse Animation")]
        [SerializeField] private float pulseSpeed     = 1.5f;
        [SerializeField] private float pulseMinScale  = 0.9f;
        [SerializeField] private float pulseMaxScale  = 1.1f;

        private float   _relativeAngle;
        private bool    _isVisible;
        private Vector3 _baseScale;
        private static readonly int EmissionColor = Shader.PropertyToID("_EmissionColor");

        private void Awake()
        {
            _baseScale = transform.localScale;
            SetVisible(false);
        }

        private void Update()
        {
            if (!_isVisible) return;

            AnimatePulse();
            UpdateGlowColor();
            BillboardKaabaIcon();
        }

        public void SetRelativeAngle(float angleDeg)
        {
            _relativeAngle = angleDeg;
        }

        public void SetVisible(bool visible)
        {
            _isVisible          = visible;
            gameObject.SetActive(visible);
        }

        private void AnimatePulse()
        {
            float t     = (Mathf.Sin(Time.time * pulseSpeed) + 1f) * 0.5f;
            float scale = Mathf.Lerp(pulseMinScale, pulseMaxScale, t);
            if (arrowMesh != null)
                arrowMesh.localScale = _baseScale * scale;
        }

        private void UpdateGlowColor()
        {
            if (glowRenderer == null) return;

            bool  aligned = Mathf.Abs(_relativeAngle) < alignedThresholdDeg
                         || Mathf.Abs(_relativeAngle - 360f) < alignedThresholdDeg;
            Color target  = aligned ? alignedColor : misalignedColor;

            glowRenderer.material.color = target;

            // URP/HDRP emission
            if (glowRenderer.material.HasProperty(EmissionColor))
                glowRenderer.material.SetColor(EmissionColor, target * 1.5f);
        }

        private void BillboardKaabaIcon()
        {
            // Keep the Kaaba icon always facing the user
            if (kaabaIconTransform != null && Camera.main != null)
            {
                kaabaIconTransform.LookAt(Camera.main.transform);
                kaabaIconTransform.Rotate(0, 180f, 0);
            }
        }
    }
}
