import React from 'react';
import ReactMarkdown from 'react-markdown';
import type { ChatMessage } from '../types';

interface ChatBubbleProps {
  message: ChatMessage;
  onSavePDF?: (data: string) => void;
}

const AIAvatar: React.FC = () => (
  <div className="ai-avatar">
    <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="white" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
      <path d="M12 2l1.09 3.26L16 6l-2.91.74L12 10l-1.09-3.26L8 6l2.91-.74L12 2z" />
      <path d="M5 12l.54 1.63L7 14.17l-1.46.37L5 16.17l-.54-1.63L3 14.17l1.46-.37L5 12z" />
      <path d="M19 12l.54 1.63L21 14.17l-1.46.37L19 16.17l-.54-1.63L17 14.17l1.46-.37L19 12z" />
    </svg>
  </div>
);

export const ChatBubble: React.FC<ChatBubbleProps> = ({ message, onSavePDF }) => {
  const isUser = message.role === 'user';

  return (
    <div className={`chat-bubble-row ${isUser ? 'user' : 'assistant'}`}>
      {isUser && <div className="spacer" />}
      {!isUser && <AIAvatar />}

      <div className={`chat-bubble-content ${isUser ? 'user' : 'assistant'}`}>
        {message.isProgress ? (
          <div className="progress-bubble">
            <div className="spinner" />
            <span>{message.content}</span>
          </div>
        ) : (
          <div className={`message-bubble ${isUser ? 'user' : 'assistant'}`}>
            <ReactMarkdown>{message.content}</ReactMarkdown>
          </div>
        )}

        {message.pdfData && (
          <button className="save-pdf-btn" onClick={() => onSavePDF?.(message.pdfData!)}>
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
              <polyline points="7 10 12 15 17 10" />
              <line x1="12" y1="15" x2="12" y2="3" />
            </svg>
            Save PDF Report
          </button>
        )}
      </div>

      {!isUser && <div className="spacer" />}
    </div>
  );
};

export const TypingIndicator: React.FC = () => (
  <div className="chat-bubble-row assistant">
    <AIAvatar />
    <div className="typing-indicator">
      <div className="typing-dot" style={{ animationDelay: '0ms' }} />
      <div className="typing-dot" style={{ animationDelay: '150ms' }} />
      <div className="typing-dot" style={{ animationDelay: '300ms' }} />
    </div>
    <div className="spacer" />
  </div>
);
