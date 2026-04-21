import React, { useState } from 'react';
import { Config } from '../config';
import { anthropicService } from '../services/anthropicService';

interface LicenseActivationProps {
  onActivated: () => void;
}

export const LicenseActivation: React.FC<LicenseActivationProps> = ({ onActivated }) => {
  const [keyInput, setKeyInput] = useState('');
  const [isValidating, setIsValidating] = useState(false);
  const [error, setError] = useState('');

  const handleActivate = async () => {
    const trimmed = keyInput.trim().toUpperCase();
    if (!trimmed) return;

    setIsValidating(true);
    setError('');

    const result = await anthropicService.validateLicenseKey(trimmed);

    if (result.valid) {
      Config.licenseKey = trimmed;
      onActivated();
    } else {
      setError(result.error || 'Invalid license key. Please try again.');
    }

    setIsValidating(false);
  };

  return (
    <div className="modal-overlay">
      <div className="modal-content api-key-modal" onClick={(e) => e.stopPropagation()}>
        <h2 className="modal-title">Welcome to BookkeepingAI</h2>
        <p className="modal-description">
          Enter your license key to activate the app. If you don't have a key, contact your administrator.
        </p>

        <div className="form-group">
          <label className="form-label">License Key</label>
          <input
            type="text"
            className="form-input mono"
            placeholder="BK-XXXX-XXXX-XXXX"
            value={keyInput}
            onChange={(e) => {
              setKeyInput(e.target.value);
              setError('');
            }}
            onKeyDown={(e) => e.key === 'Enter' && handleActivate()}
            disabled={isValidating}
            autoFocus
          />
          {error && <p className="form-error">{error}</p>}
        </div>

        <button
          className="primary-btn full-width"
          onClick={handleActivate}
          disabled={!keyInput.trim() || isValidating}
        >
          {isValidating ? 'Validating...' : 'Activate'}
        </button>

        <p className="modal-footnote">
          Your license key is stored locally on this device.
        </p>
      </div>
    </div>
  );
};

interface SettingsSheetProps {
  onClose: () => void;
  onDeactivate: () => void;
}

export const SettingsSheet: React.FC<SettingsSheetProps> = ({ onClose, onDeactivate }) => {
  const key = Config.licenseKey;
  const masked = key ? `${key.slice(0, 3)}****-****-${key.slice(-4)}` : 'None';
  const handleDeactivate = () => {
    Config.licenseKey = '';
    onDeactivate();
    onClose();
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content settings-modal" onClick={(e) => e.stopPropagation()}>
        <div className="settings-header">
          <h2 className="modal-title" style={{ margin: 0 }}>Settings</h2>
          <button className="primary-btn" onClick={onClose}>Done</button>
        </div>

        <div className="divider" />

        <div className="settings-status">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#34c759" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
          </svg>
          <div>
            <div className="settings-status-title">License Active</div>
            <div className="settings-status-subtitle">{masked}</div>
          </div>
        </div>

        <div className="divider" />

        <button
          className="deactivate-btn"
          onClick={handleDeactivate}
        >
          Deactivate License
        </button>
      </div>
    </div>
  );
};
