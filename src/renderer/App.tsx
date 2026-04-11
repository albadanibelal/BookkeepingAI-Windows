import React, { useEffect, useRef } from 'react';
import { Header } from './components/Header';
import { ChatBubble, TypingIndicator } from './components/ChatBubble';
import { FileDropZone } from './components/FileDropZone';
import { InputBar } from './components/InputBar';
import { APIKeySheet, SettingsSheet } from './components/Settings';
import { useChatViewModel } from './hooks/useChatViewModel';

const App: React.FC = () => {
  const vm = useChatViewModel();
  const chatEndRef = useRef<HTMLDivElement>(null);
  const initialized = useRef(false);

  useEffect(() => {
    if (!initialized.current) {
      initialized.current = true;
      vm.onAppear();
    }
  }, []);

  // Auto-scroll to bottom
  useEffect(() => {
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [vm.messages, vm.isSending]);

  // Handle drag-and-drop on the whole window
  const handleDrop = async (e: React.DragEvent) => {
    e.preventDefault();
    const files = Array.from(e.dataTransfer.files);
    const fileInfos: Array<{ name: string; data: string }> = [];
    for (const file of files) {
      const data = await new Promise<string>((resolve) => {
        const reader = new FileReader();
        reader.onload = () => {
          const result = reader.result as string;
          resolve(result.split(',')[1] ?? '');
        };
        reader.readAsDataURL(file);
      });
      fileInfos.push({ name: file.name, data });
    }
    vm.addFiles(fileInfos);
  };

  return (
    <div
      className="app-container"
      onDrop={handleDrop}
      onDragOver={(e) => e.preventDefault()}
    >
      <Header
        onNewChat={vm.newChat}
        onSettings={() => vm.setShowSettings(true)}
      />

      <div className="chat-area">
        <div className="chat-messages">
          {vm.messages.map((msg) => (
            <ChatBubble key={msg.id} message={msg} onSavePDF={vm.savePDF} />
          ))}
          {vm.isSending && <TypingIndicator />}
          <div ref={chatEndRef} />
        </div>
      </div>

      <FileDropZone
        files={vm.uploadedFiles}
        onAddFiles={vm.addFiles}
        onRemoveFile={vm.removeFile}
      />

      <InputBar
        text={vm.inputText}
        onTextChange={vm.setInputText}
        isSending={vm.isSending}
        hasFiles={vm.uploadedFiles.length > 0}
        filesUploading={vm.hasFilesUploading}
        onSend={vm.sendMessage}
      />

      {vm.showAPIKeyPrompt && (
        <APIKeySheet
          apiKey={vm.apiKey}
          onSave={vm.updateApiKey}
          onClose={() => vm.setShowAPIKeyPrompt(false)}
        />
      )}

      {vm.showSettings && (
        <SettingsSheet onClose={() => vm.setShowSettings(false)} />
      )}
    </div>
  );
};

export default App;
