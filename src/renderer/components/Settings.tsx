import React, { useState } from 'react';

interface APIKeySheetProps {
  apiKey: string;
  onSave: (key: string) => void;
  onClose: () => void;
}

export const APIKeySheet: React.FC<APIKeySheetProps> = ({ apiKey, onSave, onClose }) => {
  const [keyInput, setKeyInput] = useState(apiKey);

  const handleSave = () => {
    const trimmed = keyInput.trim();
    if (!trimmed) return;
    onSave(trimmed);
    onClose();
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content api-key-modal" onClick={(e) => e.stopPropagation()}>
        <h2 className="modal-title">Welcome to BookkeepingAI</h2>
        <p className="modal-description">
          Enter your Anthropic API key to get started. Your key is stored locally on your device and never shared.
        </p>

        <div className="form-group">
          <label className="form-label">API Key</label>
          <input
            type="password"
            className="form-input mono"
            placeholder="sk-ant-..."
            value={keyInput}
            onChange={(e) => setKeyInput(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && handleSave()}
          />
        </div>

        <button
          className="primary-btn full-width"
          onClick={handleSave}
          disabled={!keyInput.trim()}
        >
          Get Started
        </button>

        <p className="modal-footnote">
          Your key stays on this device. BookkeepingAI calls the Anthropic API directly.
        </p>
      </div>
    </div>
  );
};

interface SettingsSheetProps {
  onClose: () => void;
}

export const SettingsSheet: React.FC<SettingsSheetProps> = ({ onClose }) => {
  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content settings-modal" onClick={(e) => e.stopPropagation()}>
        <div className="settings-header">
          <h2 className="modal-title" style={{ margin: 0 }}>Settings</h2>
          <button className="primary-btn" onClick={onClose}>Done</button>
        </div>

        <div className="divider" />

        <div className="settings-status">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="#16a34a" stroke="none">
            <path d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" fill="none" stroke="#16a34a" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
          </svg>
          <div>
            <div className="settings-status-title">API Key Configured</div>
            <div className="settings-status-subtitle">BookkeepingAI is ready to use.</div>
          </div>
        </div>
      </div>
    </div>
  );
};
