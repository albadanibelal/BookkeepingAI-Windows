import React, { useCallback, useState } from 'react';
import type { UploadedFile } from '../types';
import { getFileIcon } from '../types';

interface FileDropZoneProps {
  files: UploadedFile[];
  onAddFiles: (files: Array<{ name: string; data: string }>) => void;
  onRemoveFile: (index: number) => void;
}

export const FileDropZone: React.FC<FileDropZoneProps> = ({ files, onAddFiles, onRemoveFile }) => {
  const [isDragOver, setIsDragOver] = useState(false);

  const handleOpenFileDialog = useCallback(async () => {
    if (window.electronAPI) {
      const result = await window.electronAPI.openFileDialog();
      if (result.length > 0) {
        onAddFiles(result.map((f) => ({ name: f.name, data: f.data })));
      }
    } else {
      // Browser fallback
      const input = document.createElement('input');
      input.type = 'file';
      input.multiple = true;
      input.accept = '.pdf,.png,.jpg,.jpeg,.webp,.csv,.xlsx,.heic';
      input.onchange = async () => {
        if (!input.files) return;
        const fileInfos: Array<{ name: string; data: string }> = [];
        for (const file of Array.from(input.files)) {
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
        onAddFiles(fileInfos);
      };
      input.click();
    }
  }, [onAddFiles]);

  const handleDrop = useCallback(
    async (e: React.DragEvent) => {
      e.preventDefault();
      setIsDragOver(false);
      const droppedFiles = Array.from(e.dataTransfer.files);
      const fileInfos: Array<{ name: string; data: string }> = [];
      for (const file of droppedFiles) {
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
      onAddFiles(fileInfos);
    },
    [onAddFiles]
  );

  const handleDragOver = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    setIsDragOver(true);
  }, []);

  const handleDragLeave = useCallback(() => {
    setIsDragOver(false);
  }, []);

  if (files.length === 0) {
    return (
      <div
        className={`file-drop-empty ${isDragOver ? 'drag-over' : ''}`}
        onClick={handleOpenFileDialog}
        onDrop={handleDrop}
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
      >
        <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
          <path d="M21.44 11.05l-9.19 9.19a6 6 0 0 1-8.49-8.49l9.19-9.19a4 4 0 0 1 5.66 5.66l-9.2 9.19a2 2 0 0 1-2.83-2.83l8.49-8.48" />
        </svg>
        <span>Attach documents</span>
        <span className="file-types">PDF, image, CSV</span>
      </div>
    );
  }

  return (
    <div
      className="file-drop-files"
      onDrop={handleDrop}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
    >
      {files.map((file, index) => (
        <div key={file.id} className="file-chip">
          {file.isUploading ? (
            <div className="chip-spinner" />
          ) : (
            <span className="chip-icon">{getFileIcon(file.mimeType)}</span>
          )}
          <span className="chip-name">{file.name}</span>
          {!file.isUploading && (
            <button className="chip-remove" onClick={() => onRemoveFile(index)}>
              <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round">
                <line x1="18" y1="6" x2="6" y2="18" />
                <line x1="6" y1="6" x2="18" y2="18" />
              </svg>
            </button>
          )}
        </div>
      ))}

      <button className="file-add-btn" onClick={handleOpenFileDialog}>
        <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round">
          <line x1="12" y1="5" x2="12" y2="19" />
          <line x1="5" y1="12" x2="19" y2="12" />
        </svg>
        Add
      </button>
    </div>
  );
};
