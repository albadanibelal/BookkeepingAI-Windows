import React, { useCallback, useRef, useEffect } from 'react';

interface InputBarProps {
  text: string;
  onTextChange: (text: string) => void;
  isSending: boolean;
  hasFiles: boolean;
  filesUploading: boolean;
  onSend: () => void;
}

export const InputBar: React.FC<InputBarProps> = ({
  text,
  onTextChange,
  isSending,
  hasFiles,
  filesUploading,
  onSend,
}) => {
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  const canSend = (text.trim().length > 0 || hasFiles) && !isSending && !filesUploading;

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        if (canSend) onSend();
      }
    },
    [canSend, onSend]
  );

  // Auto-resize textarea
  useEffect(() => {
    const el = textareaRef.current;
    if (el) {
      el.style.height = 'auto';
      el.style.height = Math.min(el.scrollHeight, 120) + 'px';
    }
  }, [text]);

  return (
    <div className="input-bar">
      <div className="input-field-wrapper">
        <textarea
          ref={textareaRef}
          className="input-field"
          placeholder="Message BookkeepingAI..."
          value={text}
          onChange={(e) => onTextChange(e.target.value)}
          onKeyDown={handleKeyDown}
          rows={1}
        />
      </div>

      <button
        className={`send-btn ${canSend ? 'active' : ''}`}
        onClick={onSend}
        disabled={!canSend}
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="white" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
          <line x1="12" y1="19" x2="12" y2="5" />
          <polyline points="5 12 12 5 19 12" />
        </svg>
      </button>
    </div>
  );
};
