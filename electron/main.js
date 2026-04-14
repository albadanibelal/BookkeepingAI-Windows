const { app, BrowserWindow, ipcMain, dialog, Menu, shell } = require('electron');
const { autoUpdater } = require('electron-updater');
const path = require('path');
const fs = require('fs');

let mainWindow = null;

// ---- Auto-Update ----

function setupAutoUpdater() {
  autoUpdater.autoDownload = true;
  autoUpdater.autoInstallOnAppQuit = true;

  autoUpdater.on('update-available', (info) => {
    if (mainWindow) {
      mainWindow.webContents.executeJavaScript(
        `document.title = 'BookkeepingAI — Downloading update v${info.version}...'`
      );
    }
  });

  autoUpdater.on('download-progress', (progress) => {
    if (mainWindow) {
      const pct = Math.round(progress.percent);
      mainWindow.webContents.executeJavaScript(
        `document.title = 'BookkeepingAI — Downloading update ${pct}%'`
      );
      mainWindow.setProgressBar(progress.percent / 100);
    }
  });

  autoUpdater.on('update-downloaded', () => {
    if (mainWindow) {
      mainWindow.setProgressBar(-1); // clear progress bar
      mainWindow.webContents.executeJavaScript(
        `document.title = 'BookkeepingAI'`
      );
    }
    dialog.showMessageBox(mainWindow, {
      type: 'info',
      title: 'Update Ready',
      message: 'A new version of BookkeepingAI has been downloaded. Restart now to update?',
      buttons: ['Restart', 'Later'],
    }).then((result) => {
      if (result.response === 0) {
        autoUpdater.quitAndInstall();
      }
    });
  });

  autoUpdater.on('update-not-available', () => {
    if (mainWindow) {
      mainWindow.webContents.executeJavaScript(
        `document.title = 'BookkeepingAI'`
      );
    }
  });

  autoUpdater.on('error', (err) => {
    console.log('Auto-update error:', err.message);
    if (mainWindow) {
      mainWindow.setProgressBar(-1);
      mainWindow.webContents.executeJavaScript(
        `document.title = 'BookkeepingAI'`
      );
      dialog.showMessageBox(mainWindow, {
        type: 'error',
        title: 'Update Failed',
        message: 'Failed to download update. Please check your internet connection and try again from Help > Check for Updates.',
      });
    }
  });

  // Check for updates (silently)
  autoUpdater.checkForUpdatesAndNotify();
}

// ---- Menu ----

function createMenu() {
  const template = [
    {
      label: 'File',
      submenu: [
        { role: 'quit' },
      ],
    },
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ],
    },
    {
      label: 'View',
      submenu: [
        { role: 'reload' },
        { role: 'toggleDevTools' },
        { type: 'separator' },
        { role: 'zoomIn' },
        { role: 'zoomOut' },
        { role: 'resetZoom' },
      ],
    },
    {
      label: 'Help',
      submenu: [
        {
          label: 'Contact Us',
          click: () => {
            shell.openExternal('https://belal-albadani-portfolio-470863874819.us-west1.run.app/#about');
          },
        },
        {
          label: 'Check for Updates',
          click: () => {
            autoUpdater.checkForUpdatesAndNotify();
          },
        },
      ],
    },
  ];

  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}

// ---- Window ----

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 900,
    height: 700,
    minWidth: 700,
    minHeight: 550,
    title: 'BookkeepingAI',
    backgroundColor: '#f3f3f3',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  const isDev = !app.isPackaged;
  if (isDev) {
    mainWindow.loadURL('http://localhost:5173');
  } else {
    mainWindow.loadFile(path.join(__dirname, '..', 'dist', 'renderer', 'index.html'));
  }

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

app.whenReady().then(() => {
  createMenu();
  createWindow();
  // Only check for updates in production
  if (app.isPackaged) {
    setupAutoUpdater();
  }
});

app.on('window-all-closed', () => {
  app.quit();
});

app.on('activate', () => {
  if (mainWindow === null) createWindow();
});

// IPC: Open file dialog
ipcMain.handle('open-file-dialog', async () => {
  if (!mainWindow) return [];
  const result = await dialog.showOpenDialog(mainWindow, {
    properties: ['openFile', 'multiSelections'],
    filters: [
      {
        name: 'Financial Documents',
        extensions: ['pdf', 'png', 'jpg', 'jpeg', 'webp', 'csv', 'xlsx', 'heic'],
      },
    ],
    message: 'Select financial documents to analyze',
  });
  if (result.canceled) return [];

  return result.filePaths.map((filePath) => {
    const data = fs.readFileSync(filePath);
    return {
      name: path.basename(filePath),
      path: filePath,
      data: data.toString('base64'),
      size: data.length,
    };
  });
});

// IPC: Save PDF
ipcMain.handle('save-pdf', async (_event, base64Data, defaultName) => {
  if (!mainWindow) return false;
  const result = await dialog.showSaveDialog(mainWindow, {
    defaultPath: defaultName,
    filters: [{ name: 'PDF', extensions: ['pdf'] }],
  });
  if (result.canceled || !result.filePath) return false;
  const buffer = Buffer.from(base64Data, 'base64');
  fs.writeFileSync(result.filePath, buffer);
  return true;
});
