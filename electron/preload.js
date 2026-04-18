const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  openFileDialog: () => ipcRenderer.invoke('open-file-dialog'),
  savePDF: (base64Data, defaultName) => ipcRenderer.invoke('save-pdf', base64Data, defaultName),
  getDeviceInfo: () => ipcRenderer.invoke('get-device-info'),
});
