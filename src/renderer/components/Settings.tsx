import React, { useState } from 'react';
import { Config, getVendorMappings, saveVendorMappings } from '../config';
import { anthropicService } from '../services/anthropicService';
import type { VendorMapping } from '../types';

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
  const [vendors, setVendors] = useState<VendorMapping[]>(getVendorMappings());

  const handleDeactivate = () => {
    Config.licenseKey = '';
    onDeactivate();
    onClose();
  };

  const handleDeleteVendor = (vendorName: string) => {
    const updated = vendors.filter(v => v.vendor !== vendorName);
    setVendors(updated);
    saveVendorMappings(updated);
  };

  const handleClearAll = () => {
    setVendors([]);
    saveVendorMappings([]);
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

        {/* Vendor Mappings Section */}
        <div style={{ padding: '0 0 12px 0' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
            <div style={{ fontSize: 13, fontWeight: 600, color: '#1a3a5c' }}>
              Learned Vendors ({vendors.length})
            </div>
            {vendors.length > 0 && (
              <button
                onClick={handleClearAll}
                style={{
                  fontSize: 11, color: '#dc2626', background: 'none', border: 'none',
                  cursor: 'pointer', padding: '2px 6px', borderRadius: 4,
                }}
              >
                Clear All
              </button>
            )}
          </div>
          <div style={{ fontSize: 11, color: '#6b7280', marginBottom: 8 }}>
            Vendors are learned automatically from each report. They ensure consistent classification across runs.
          </div>
          {vendors.length === 0 ? (
            <div style={{ fontSize: 12, color: '#9ca3af', fontStyle: 'italic', padding: '8px 0' }}>
              No vendors learned yet. Run a report to start building your vendor list.
            </div>
          ) : (
            <div style={{ maxHeight: 200, overflowY: 'auto', borderRadius: 6, border: '1px solid #e5e7eb' }}>
              {vendors.map((v) => (
                <div
                  key={v.vendor}
                  style={{
                    display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                    padding: '6px 10px', borderBottom: '1px solid #f3f4f6', fontSize: 11,
                  }}
                >
                  <div style={{ flex: 1 }}>
                    <span style={{ fontWeight: 600, color: '#1a3a5c' }}>{v.vendor}</span>
                    <span style={{ color: '#6b7280', marginLeft: 6 }}>{v.category}</span>
                    <span style={{
                      marginLeft: 6, padding: '1px 5px', borderRadius: 3, fontSize: 10,
                      background: v.taxability === 'Taxable' ? '#fef2f2' : v.taxability === 'Non-Taxable' ? '#f0fdf4' : '#fffbeb',
                      color: v.taxability === 'Taxable' ? '#dc2626' : v.taxability === 'Non-Taxable' ? '#16a34a' : '#ca8a04',
                    }}>
                      {v.taxability}
                    </span>
                  </div>
                  <button
                    onClick={() => handleDeleteVendor(v.vendor)}
                    style={{
                      background: 'none', border: 'none', cursor: 'pointer',
                      color: '#9ca3af', fontSize: 14, padding: '0 4px', lineHeight: 1,
                    }}
                    title="Remove vendor"
                  >
                    ×
                  </button>
                </div>
              ))}
            </div>
          )}
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
