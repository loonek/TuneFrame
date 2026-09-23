const WS_URL = "ws://localhost:8765";
let ws = null;
let contentPort = null;

// Connects ws://localhost:8765 with the bridge. Acts as a relay
function connectWS()
{
  if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) return;
  ws = new WebSocket(WS_URL);
  ws.onclose = () =>
  {
    ws = null;
    setTimeout(connectWS, 2000);
  };
  ws.onerror = () =>
  {
    try { ws.close(); } catch (e) {}
  };
  ws.onmessage = (ev) =>
  {
    if (!contentPort) return;
    try
    {
      const msg = JSON.parse(ev.data);
      if (msg.t === "cmd" && msg.a === "search" && contentPort.sender && contentPort.sender.tab)
      {
        chrome.tabs.update(contentPort.sender.tab.id, { active: true });
        chrome.windows.update(contentPort.sender.tab.windowId, { focused: true });
      }
    }
    catch (e) {}
    contentPort.postMessage(ev.data);
  }
}

connectWS();

// Keeps the MV3 service worker (and its WebSocket) alive across idle periods
chrome.alarms.create("keepAlive", { periodInMinutes: 0.5 });
chrome.alarms.onAlarm.addListener(() =>
{
  if (!ws || ws.readyState !== WebSocket.OPEN) connectWS();
  else try { ws.send(JSON.stringify({ t: "ping" })); } catch (e) {}
});

chrome.runtime.onConnect.addListener((port) =>
{
  contentPort = port;
  port.onMessage.addListener((msg) =>
  {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(msg));
  });
  port.onDisconnect.addListener(() =>
  {
    contentPort = null;
  });
});
