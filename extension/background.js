const WS_URL = "ws://localhost:8765";
let ws = null;
let contentPort = null;

function connectWS()
{
  ws = new WebSocket(WS_URL);
  ws.onmessage = (ev) =>
  {
    if (contentPort) contentPort.postMessage(ev.data);
  };
  ws.onclose = () =>
  {
    ws = null;
    setTimeout(connectWS, 2000);
  };
  ws.onerror = () =>
  {
    try { ws.close(); } catch (e) {}
  };
}

connectWS();

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
