const port = chrome.runtime.connect();

window.addEventListener("message", (e) =>
{
  if (e.source !== window) return;
  const d = e.data;
  if (!d || d.source !== "ytmc" || d.dir !== "up") return;
  port.postMessage(d.payload);
});

port.onMessage.addListener((data) =>
{
  let cmd;
  try { cmd = JSON.parse(data); } catch (e) { return; }
  window.postMessage({ source: "ytmc", dir: "down", payload: cmd }, "*");
});
