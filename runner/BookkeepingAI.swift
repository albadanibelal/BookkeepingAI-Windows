import Cocoa
import WebKit

class AppDelegate: NSObject, NSApplicationDelegate, WKUIDelegate {
    var window: NSWindow!
    var webView: WKWebView!

    func applicationDidFinishLaunching(_ notification: Notification) {
        let config = WKWebViewConfiguration()
        config.preferences.setValue(true, forKey: "allowFileAccessFromFileURLs")
        config.preferences.setValue(true, forKey: "developerExtrasEnabled")

        webView = WKWebView(frame: .zero, configuration: config)
        webView.uiDelegate = self

        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 950, height: 720),
            styleMask: [.titled, .closable, .resizable, .miniaturizable],
            backing: .buffered,
            defer: false
        )
        window.title = "BookkeepingAI"
        window.center()
        window.contentView = webView
        window.makeKeyAndOrderFront(nil)
        window.minSize = NSSize(width: 700, height: 550)

        // Set the app icon
        let basePath = (CommandLine.arguments[0] as NSString).deletingLastPathComponent
        let iconPath = (basePath as NSString).appendingPathComponent("../assets/icon.png")
        if let icon = NSImage(contentsOfFile: iconPath) {
            NSApp.applicationIconImage = icon
        }

        if let url = URL(string: "http://localhost:5173") {
            webView.load(URLRequest(url: url))
        }
    }

    // Handle file input <input type="file"> — open native file picker
    func webView(_ webView: WKWebView,
                 runOpenPanelWith parameters: WKOpenPanelParameters,
                 initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping ([URL]?) -> Void) {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = parameters.allowsMultipleSelection
        panel.canChooseDirectories = parameters.allowsDirectories
        panel.canChooseFiles = true
        panel.allowedContentTypes = [
            .pdf, .png, .jpeg,
            .commaSeparatedText,
            .data
        ]
        panel.message = "Select financial documents to analyze"

        panel.begin { result in
            if result == .OK {
                completionHandler(panel.urls)
            } else {
                completionHandler(nil)
            }
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.run()
